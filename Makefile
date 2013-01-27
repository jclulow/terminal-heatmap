

CC=gcc


all:	heatmap

heatmap:	heatmap.c
	$(CC) -o $@ $<

