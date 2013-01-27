

CC=gcc


all:	heatmap

heatmap:	heatmap.c
	$(CC) -lm -o $@ $<

clean:
	rm -f heatmap
