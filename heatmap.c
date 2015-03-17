

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#if defined(__SVR4) && defined(__sun)
#include <stropts.h>
#include <sys/types.h>
#else
typedef enum { B_FALSE, B_TRUE } boolean_t;
#endif
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <math.h>

#define	ESC	"\x1b"
#define	CSI	ESC "["
#define	NOCURS	CSI "?25l"
#define	CURS	CSI "?25h"
#define	RST	CSI "0m"
#define	REV	CSI "7m"
#define	CLRSCR	CSI "H" CSI "J"

/* NB: The darkest gray is probably too dark. */
#define	GRAYMIN	(232 + 1)
#define	GRAYMAX	255
#define	GRAYRNG	(GRAYMAX - GRAYMIN)

#define	LEGEND_WIDTH	10

#define	TITLE	"T E R M I N A L   H E A T M A P"


int h;
int w;

int *bucket_vals = NULL;
int bucket_count = -1;

int debug = 0;


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
	int len = strlen(title) + 6;
	int offs = (w - len) / 2;

	moveto(offs, 1);
	fprintf(stdout, REV "   %s   " RST, title);
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
loglinear_buckets(int base, int minord, int maxord)
{
	int i;
	int steps_per_order, steps, order;

	/*
	 * Decide how many linear steps we can have per order.
	 */
	steps_per_order = (bucket_count - 1) / (maxord - minord);
	order = (int) pow(base, minord);
	steps = 0;

	for (i = 0; i < bucket_count; i++) {
		if (steps > steps_per_order) {
			steps = 1;
			order *= base;
		}

		bucket_vals[i] = (i == 0 ? 0 : order + (base * order) *
		    steps / steps_per_order);

		steps++;
	}
}

void
linear_buckets(int min, int max)
{
	int i;

	for (i = 0; i < bucket_count; i++) {
		bucket_vals[i] = i * (max - min) / bucket_count + min;
	}
}

void
allocate_buckets()
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
new_row_column(int bucket, int value, int colour)
{
	int newcol = w - LEGEND_WIDTH - 1;
	int y = h - 1 - bucket;

	/*
	 * Shift this bucket to the left:
	 */
	fprintf(stdout, CSI "%d;4H", y); /* move afore the line */
	fprintf(stdout, CSI "1P"); /* truncate on the left */

	/*
	 * Write heatmap block:
	 */
	fprintf(stdout, CSI "%d;%dH", y, newcol); /* move to right column */
	xcb(colour);
	fprintf(stdout, " " RST);

	/*
	 * Write legend:
	 */
	if (debug > 0 || bucket != bucket_count - 2 && (bucket % 3 == 0 ||
	    bucket == bucket_count - 1))
		fprintf(stdout, " %d", bucket_vals[bucket]);

	/*
	 * Print values for debugging:
	 */
	if (debug > 0)
		fprintf(stdout, CSI "%d;1H" RST "%d", y, value);
}

void
new_row(int *vals)
{
	char *timestr = time_string();
	int remaining = bucket_count;
	int bucket;
	int *dvlist = empty_buckets();
	int distinct_vals = 0, rem_distinct_vals;

	moveto(-(strlen(timestr)), 1);
	fprintf(stdout, "%s", timestr);

	/*
	 * Do Rank-based Colouring, as described here:
	 *   http://dtrace.org/blogs/dap/2011/06/20/heatmap-coloring/
	 */
	/*
	 * 1. Print Black for all zero-valued buckets.
	 */
	for (bucket = 0; bucket < bucket_count; bucket++) {
		if (vals[bucket] == 0) {
			new_row_column(bucket, 0, 0);
			remaining--;
		} else {
			/*
			 * Record distinct non-zero bucket values.
			 */
			int c;
			for (c = 0; c < bucket_count; c++) {
				if (dvlist[c] == vals[bucket]) {
					/* seen this one already. */
					break;
				} else if (dvlist[c] == 0) {
					/* ok, mark it down. */
					dvlist[c] = vals[bucket];
					distinct_vals++;
					break;
				}
			}
		}
	}

	if (remaining < 1)
		goto out;

	/*
	 * 2. Draw all buckets having the same value with the same colour
	 *    on a rank-based ramp from GRAY_MIN to GRAY_MAX.
	 */
	rem_distinct_vals = distinct_vals;
	while (remaining > 0 && rem_distinct_vals > 0) {
		int dv;
		int maxdv = 0;
		int colour;
		/*
		 * Find next-largest distinct bucket value:
		 */
		for (dv = 0; dv < bucket_count; dv++) {
			if (dvlist[dv] > dvlist[maxdv]) {
				maxdv = dv;
			}
		}
		/*
		 * Determine colour:
		 */
		if (distinct_vals < 2) {
			/*
			 * If all buckets are the same value, just use the
			 * highest intensity.
			 */
			colour = GRAYMAX;
		} else {
			colour = GRAYMIN + (GRAYMAX - GRAYMIN) *
			    (rem_distinct_vals - 1) / (distinct_vals - 1);
		}
		/*
		 * Draw all buckets with that value:
		 */
		for (bucket = 0; bucket < bucket_count; bucket++) {
			if (vals[bucket] == dvlist[maxdv]) {
				new_row_column(bucket, vals[bucket], colour);
				remaining--;
			}
		}
		/*
		 * Clear out the distinct value.
		 */
		dvlist[maxdv] = 0;
		rem_distinct_vals--;
	}

out:
	free(dvlist);
	free(vals);
}

typedef enum {
	PS_REST = 1,
	PS_VALUE,
	PS_COUNT,
	PS_DONE
} parser_state_t;

int *
line_to_row(char *line)
{
	parser_state_t state;
	int *row = empty_buckets();
	char *cp = line, *mark = NULL;
	boolean_t commit = B_FALSE;
	int value = -1, count = 1;

	for (state = PS_REST; state != PS_DONE; cp++) {
		if (*cp == '\n' || *cp == '\r') {
			/*
			 * fgets() may leave trailing new-lines.  Treat them
			 * as string terminators.
			 */
			*cp = '\0';
		}
		switch (state) {
		case PS_REST:
			if (*cp >= '0' && *cp <= '9') {
				state = PS_VALUE;
				mark = cp;
			} else if (*cp == '\0') {
				state = PS_DONE;
			} else if (*cp != ' ' && *cp != '\t') {
				/*
				 * Unexpected input!
				 */
				goto error;
			}
			break;
		case PS_VALUE:
			if (*cp >= '0' && *cp <= '9') {
				break;
			} else if (*cp == '\0' || *cp == ' ' || *cp == '\t') {
				state = (*cp == '\0' ? PS_DONE : PS_REST);
				*cp = '\0';
				value = atoi(mark);
				count = 1;
				commit = B_TRUE;
			} else if (*cp == '*') {
				state = PS_COUNT;
				*cp = '\0';
				value = atoi(mark);
				mark = cp + 1;
			} else {
				/*
				 * Unexpected input!
				 */
				fprintf(stderr, CLRSCR CURS "\nerror two\n");
				exit(1);
				goto error;
			}
			break;
		case PS_COUNT:
			if (*cp >= '0' && *cp <= '9') {
				break;
			} else if (*cp == '\0' || *cp == ' ' || *cp == '\t') {
				state = (*cp == '\0' ? PS_DONE : PS_REST);
				*cp = '\0';
				count = atoi(mark);
				commit = B_TRUE;
			} else {
				/*
				 * Unexpected input!
				 */
				fprintf(stderr, CLRSCR CURS "\nerror three\n");
				exit(1);
				goto error;
			}
			break;
		default:
			goto error;
		}
		if (commit == B_TRUE) {
			int bucket = find_bucket(value);
			row[bucket] += count;
			commit = B_FALSE;
		}
	}
	return (row);

error:
	free(row);
	return (NULL);
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
	int c;
	int opt_base = -1, opt_min = -1, opt_max = -1;
	int opt_lin = 0, opt_loglin = 0;
	char *opt_title = NULL;
	char *linebuf = NULL;
	size_t linebufsz = 0;

	/*
	 * Process flags...
	 */
	while ((c = getopt(argc, argv, ":b:DlLm:M:t:")) != -1) {
		switch (c) {
		case 'l':
			opt_lin++;
			break;
		case 'L':
			opt_loglin++;
			break;
		case 'b':
			opt_base = atoi(optarg);
			break;
		case 'm':
			opt_min = atoi(optarg);
			break;
		case 'M':
			opt_max = atoi(optarg);
			break;
		case 'D':
			debug++;
			break;
		case 't':
			opt_title = strdup(optarg);
			break;
		case ':':
			fprintf(stderr, "Option -%c requires an operand\n",
			    optopt);
			exit(1);
			break;
		case '?':
			fprintf(stderr, "Option -%c not recognised\n",
			    optopt);
			exit(1);
			break;
		}
		if (opt_lin > 0 && opt_loglin > 0) {
			fprintf(stderr, "Options -l and -L are mutually "
			    "exclusive.\n");
			exit(1);
		}
	}

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

	allocate_buckets();
	if (opt_loglin) {
		/*
		 * Use log-linear bucket ranges, similar to DTrace
		 * llquantize().
		 */
		int base = opt_base > 0 ? opt_base : 10;
		int min = opt_min > 0 ? opt_min : 2;
		int max = opt_max > 0 ? opt_max : 6;

		loglinear_buckets(base, min, max);
	} else {
		/* Assume linear, using sane defaults for mpstat(1). */
		int min = opt_min > 0 ? opt_min : 0;
		int max = opt_max > 0 ? opt_max : 100;

		linear_buckets(min, max);
	}

	/*
	 * Set up the screen:
	 */
	fprintf(stdout, CLRSCR NOCURS);
	print_title(opt_title != NULL ? opt_title : TITLE);
	print_time_markers();

	/*
	 * Read and tokenise tab/space-separated input lines of the form:
	 *    56  23  23  54 55 22 99 100 0 22 0 0 100 ....
	 * where each integer value will increment the bucket it fits in.
	 */
	for (;;) {
		int *row;
		ssize_t ret;

		errno = 0;
		if ((ret = getline(&linebuf, &linebufsz, stdin)) < 0) {
			if (errno == 0) {
				/*
				 * End of input stream.  Clean up the terminal
				 * and exit gracefully.
				 */
				moveto(0, -1);
				fprintf(stdout, CURS "\n");
				exit(0);
			}

			fprintf(stdout, CLRSCR CURS);
			fprintf(stderr, "ERROR: %s\n", strerror(errno));
			exit(1);
		}

		if ((row = line_to_row(linebuf)) == NULL) {
			fprintf(stdout, CLRSCR CURS);
			fprintf(stderr, "Unexpected input formatting; "
			    "aborting.\n");
			exit(1);
		}

		new_row(row);
	}
}

