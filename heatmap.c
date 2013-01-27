

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#if defined(__SVR4) && defined(__sun)
#include <stropts.h>
#endif
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#define	ESC	"\x1b"
#define	CSI	ESC "["
#define	NOCURS	CSI "?25l"
#define	CURS	CSI "?25h"
#define	RST	CSI "0m"
#define	REV	CSI "7m"
#define	CLRSCR	CSI "H" CSI "J"

#define	GRAYMIN	232
#define	GRAYMAX	255
#define	GRAYRNG	(GRAYMAX - GRAYMIN)

#define	LEGEND_WIDTH	6

#define	BUFFER_SIZE	4096

#define	TITLE	"   T E R M I N A L   H E A T M A P   "


int h;
int w;

int *bucket_vals = NULL;
int bucket_count = -1;



void
xcb(int idx)
{
	fprintf(stdout, CSI "48;5;%dm", idx);
}

void
xcf(int idx)
{
	fprintf(stdout, CSI "38;5;%dm", idx);
}

void
moveto(int x, int y)
{
	if (x < 0)
		x = w + x + 1;
	if (y < 0)
		y = h + y + 1;
	fprintf(stdout, CSI "%d;%dH", y, x);
}

void
print_title(char *title)
{
	int len = strlen(title);
	int offs = (w - len) / 2;

	moveto(offs, 1);
	fprintf(stdout, REV "%s" RST, TITLE);
}

void
print_time_markers(void)
{
	int offs;
	int mark = 0;
	char marker[100];

	for (offs = w - LEGEND_WIDTH; offs >= 1; offs -= 10) {
		int toffs = offs;

		(void) snprintf(marker, 100, "%ds|", mark);

		toffs -= strlen(marker);
		if (toffs < 1)
			break;

		moveto(toffs, -1);
		fprintf(stdout, "%s", marker);

		mark += 10;
	}
}

void
populate_buckets(int min, int max)
{
	int i;

	if (bucket_vals != NULL || bucket_count != -1)
		abort();

	bucket_count = h - 2;
	bucket_vals = malloc(sizeof(*bucket_vals) * bucket_count);
	if (bucket_vals == NULL) {
		fprintf(stderr, "bucket values: %s\n", strerror(errno));
		exit(1);
	}

	for (i = 0; i < bucket_count; i++) {
		bucket_vals[i] = i * (max - min) / bucket_count + min;
	}
}

int *
empty_buckets(void)
{
	int *ret = calloc(bucket_count, sizeof (*bucket_vals));

	if (ret == NULL) {
		fprintf(stderr, "empty buckets: %s\n", strerror(errno));
		exit(1);
	}

	return (ret);
}

int
find_bucket(int val)
{
	int bucket = bucket_count - 1;
	while (bucket > 0) {
		if (val >= bucket_vals[bucket])
			return (bucket);
		bucket--;
	}
	return (0);
}

char *
time_string(void)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	static char out[100];

	strftime(out, sizeof (out), "%H:%M:%S", tm);

	return (out);
}

void
new_row(int *vals)
{
	char *timestr = time_string();
	int newcol = w - LEGEND_WIDTH - 1;
	int bucket = 0;

	moveto(-(strlen(timestr)), 1);
	fprintf(stdout, "%s", timestr);

	for (bucket = 0; bucket < bucket_count; bucket++) {
		int y = h - 1 - bucket;
		fprintf(stdout, CSI "%d;4H", y); /* move afore the line */
		fprintf(stdout, CSI "1P"); /* truncate on the left */

		/*
		 * Write heatmap block:
		 */
		fprintf(stdout, CSI "%d;%dH", y, newcol); /* move to right column */
		xcb(GRAYMIN + 2 * vals[bucket]);
		fprintf(stdout, " " RST);

		/*
		 * Write legend:
		 */
		if (bucket % 3 == 0 || bucket == bucket_count - 1)
			fprintf(stdout, " %d", bucket_vals[bucket]);
	}

	free(vals);
}

int *
line_to_row(char *line)
{
	char *last = NULL;
	char *pos = line;
	int *row = empty_buckets();

	while (*pos != '\0' && *pos != '\n') {
		if ((*pos >= '0' && *pos <= '9') && last == NULL) {
			last = pos;
		} else if ((*pos == ' ' || *pos == '\t') && last != NULL) {
			int val, bkt;
			*pos = '\0';
			val = atoi(last);
			bkt = find_bucket(val);
			row[bkt]++;
			last = NULL;
		}
		pos++;
	}
	if (last != NULL) {
		int val, bkt;
		*pos = '\0';
		val = atoi(last);
		bkt = find_bucket(val);
		row[bkt]++;
	}

	return (row);
}

static void
intr(int sig)
{
	fprintf(stdout, CLRSCR CURS);
	exit(0);
}

void
get_terminal_size()
{
#if defined(TIOCGWINSZ)
	struct winsize tsz;
#elif defined(TIOCGSIZE)
	struct ttysize tsz;
#endif

#if defined(TIOCGWINSZ)
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &tsz) == -1) {
		fprintf(stderr, "could not get screen size!\n");
		exit(1);
	}

        h = tsz.ws_row;
        w = tsz.ws_col;

#elif defined(TIOCGSIZE)
	if (ioctl(STDOUT_FILENO, TIOCGSIZE, &tsz) == -1) {
		fprintf(stderr, "could not get screen size!\n");
		exit(1);
	}

	h = tsz.ts_lines;
	w = tsz.ts_cols;


#else
	if (getenv("COLUMNS") == NULL || getenv("LINES") == NULL) {
		fprintf(stderr, "could not get screen size!\n");
		fprintf(stderr, "try:  export COLUMNS LINES;\n");
		exit(1);
	}
	h = atoi(getenv("LINES"));
	w = atoi(getenv("COLUMNS"));
#endif
}

int
main(int argc, char **argv)
{
	char *line_buffer = malloc(BUFFER_SIZE);

	/*
	 * Signals...
	 */
	(void) signal(SIGINT, intr);
	(void) signal(SIGHUP, intr);
	(void) signal(SIGTERM, intr);

	if (isatty(STDIN_FILENO)) {
		fprintf(stderr, "stdin is a tty; please redirect me "
		    "appropriate input from source software.\n");
		exit(1);
	}
	get_terminal_size();

	/*
	 * Line-buffer our input; don't buffer our output.
	 */
	setvbuf(stdin, NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (line_buffer == NULL) {
		fprintf(stderr, "line buffer: %s\n", strerror(errno));
		exit(1);
	}

	populate_buckets(0, 100);

	/*
	 * Set up the screen:
	 */
	fprintf(stdout, CLRSCR NOCURS);
	print_title(TITLE);
	print_time_markers();

	/*
	 * Read and tokenise tab/space-separated input lines of the form:
	 *    56  23  23  54 55 22 99 100 0 22 0 0 100 ....
	 * where each integer value will increment the bucket it fits in.
	 */
	for (;;) {
		char *buf = fgets(line_buffer, BUFFER_SIZE, stdin);

		if (buf == NULL)
			break;

		new_row(line_to_row(buf));
	}

	/* XXX cleanup terminal here */
	fprintf(stdout, CLRSCR CURS);
}

