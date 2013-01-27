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
0 and 100.  The most natural usage is to take input from _mpstat(1)_
and spit out CPU Usage figures for all CPUs, once per second,
using the provided ```formatters/mpstat``` script.  For example:

```bash
ssh someserver mpstat 1 | ./formatters/mpstat | ./heatmap
```
