

CC=gcc


all:	heatmap

heatmap:	heatmap.c
	$(CC) -o $@ $< -lm

clean:
	rm -f heatmap
