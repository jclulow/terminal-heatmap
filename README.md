# terminal-heatmap

An experiment in terminal-based heat maps.  For example, witness
the majesty of this _mpstat(1)_ display of a 16-CPU zone in the JPC:

![awesome heatmap](http://i.imgur.com/23Yps0g.png)

## The node.js Version

See: _heatmap.js_.  It's kind of broken at the moment.

## The C Version

This version accepts input on ```stdin``` of the form ...

```
10 8 6 6 3 3 4 4 3 4 2 3 4 5 3 5
46 2 6 6 4 1 57 2 10 0 3 2 3 8 3 12
97 3 6 8 5 1 2 4 0 11 1 1 4 2 11 4
```

... where each line is some number whitespace-separated integers between
0 and 100.  The integers will be split up into buckets and the count of
integers in each bucket determines that bucket's intensity in the
heatmap.

The most natural usage is to take input from _mpstat(1)_ and spit out
CPU Usage figures for all CPUs, once per second, using the provided
```formatters/mpstat``` script.  For example:

```bash
ssh someserver mpstat 1 | ./formatters/mpstat | ./heatmap
```

### Bucket Distributions

You can decide the Y-axis for the heat map with a few options.

#### Linear (-l)

The default distribution of values between buckets is linear, with a
minimum value of 0 and a maximum of 100.  This is equivalent to:

```
someprogram | ./heatmap -l -m 0 -M 100
```

#### Log-Linear (-L)

In a similar fashion to
[DTrace's llquantize()](http://dtrace.org/blogs/bmc/2011/02/08/llquantize/)
you can specify a log distribution with linear buckets within each order
of magnitude.  For instance: if I/O Latency is expressed in
microseconds, it may be useful to draw a heatmap with a base of 10, from
 the second up to the sixth order of magnitude.  i.e.

```
iolatency.d | ./someformatter | ./heatmap -L -b 10 -m 2 -M 6
```

Example output from iolatency.d during a ZFS pool scrub:

![zfs scrub heatmap](http://i.imgur.com/WqodZ9F.png)

### Colouring

In the most recent C-based version I have used a Rank-based Colouring, as
described in
[this blog post](http://dtrace.org/blogs/dap/2011/06/20/heatmap-coloring/)
by Dave Pacheco.
