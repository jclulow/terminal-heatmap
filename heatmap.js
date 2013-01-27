#!/usr/bin/env node

var ESC = '\x1b';
var CSI = ESC + '[';
var RST = CSI + 'm';

var child_process = require('child_process');
var spawn = child_process.spawn;

function XCF(ix) { return CSI + '38;5;' + ix + 'm'; }
function XCB(ix) { return CSI + '48;5;' + ix + 'm'; }

function wr(str)
{
  process.stdout.write(str);
}

/* Clear screen: */
wr(CSI + 'H' + CSI + 'J');

var i;

var MIN = 232;
var MAX = 255;
var RNG = 255 - 232;

var LEGWIDTH = 6;

var title = '  T E R M I N A L   H E A T M A P  ';
var titleoff = Math.floor((process.stdout.columns - title.length) / 2);
wr(CSI + '1;' + titleoff + 'H' + CSI + '7m' + title + CSI + '0m');

wr(CSI + '?25l'); /* hide cursor */

function moveto(x, y) {
  if (x < 0)
    x = process.stdout.columns + x + 1;
  if (y < 0)
    y = process.stdout.rows + y + 1;
  wr(CSI + y + ';' + x + 'H');
}

/*
 * print time markers
 */
var STARTOFFS = process.stdout.columns - LEGWIDTH;
var mm = 0;
for (;;) {
  var marker = mm + 's|';
  STARTOFFS -= marker.length;
  if (STARTOFFS < 1)
    break;
  moveto(STARTOFFS, -1);
  wr(marker);
  STARTOFFS -= 10;
  mm += 10;
}

/*
 * configure buckets
 */
var BUCKETS = [];
var MAXBUCKET = 100;
for (var i = 0; i < process.stdout.rows - 2; i++) {
  BUCKETS[i] = Math.floor(i * MAXBUCKET / (process.stdout.rows - 2));
}

/*
wr('BUCKETS:\n\n' + BUCKETS.join(' ; ') + '\n\n');
process.exit(0);*/

/*
 * print value markers
 */
var VALMARKS = [];
var STARTOFFS = process.stdout.rows - 1;
for (;;) {
  var marker = '' + mm;
  if (STARTOFFS < 2)
    break;
  //moveto(process.stdout.columns - LEGWIDTH + 1, STARTOFFS);
  //wr(marker);
  VALMARKS[STARTOFFS] = marker;
  STARTOFFS -= 3;
  mm *= 2;
}


/*
for (var y = 2; y < process.stdout.rows; y++) {
  wr(CSI + y + ';4H');
  i = (y - 2) % RNG;
  for (var x = 4; x < process.stdout.columns - LEGWIDTH; x++) {
    wr(XCB(MIN + i) + ' ');
    i = (i + 1) % RNG;
  }
}
wr(RST);
i = (process.stdout.columns - 6) % RNG;
*/
i = 0;

function intrv()
{
  var timestr = (new Date()).toTimeString().replace(/ .*/,'');
  moveto(-(timestr.length), 1);
  wr(timestr);

  var ii = i;
  for (var y = 2; y < process.stdout.rows; y++) {
    wr(CSI + y + ';4H');
    wr(CSI + '1P');
    wr(CSI + y + ';' + (process.stdout.columns - LEGWIDTH - 1) + 'H');
    wr(XCB(MIN + ii) + ' ' + RST);
    wr(' ' + BUCKETS[process.stdout.rows - y - 1]);
    ii = (ii + 1) % RNG;
  }
  i = (i + 1) % RNG;
}
//setInterval(intrv, 1000);

function newRow(bucketvals)
{
  var timestr = (new Date()).toTimeString().replace(/ .*/,'');
  moveto(-(timestr.length), 1);
  wr(timestr);

  var newcol = process.stdout.columns - LEGWIDTH - 1;
  for (var bucket = 0; bucket < BUCKETS.length; bucket++) {
    var y = process.stdout.rows - 1 - bucket;
    wr(CSI + y + ';4H'); // move afore the line
    wr(CSI + '1P');      // truncate on left
    wr(CSI + y + ';' + newcol + 'H'); // move to right hand column

    // write heatmap block:
    wr(XCB(MIN + 2 * bucketvals[bucket]) + ' ' + RST);

    if (bucket % 3 === 0 || bucket === BUCKETS.length - 1)
      wr(' ' + BUCKETS[bucket]); // write legend
  }

/*
  for (var y = 2; y < process.stdout.rows; y++) {
    wr(CSI + y + ';4H');
    wr(CSI + '1P');
    wr(CSI + y + ';' + (process.stdout.columns - LEGWIDTH - 1) + 'H');
    wr(XCB(MIN + ii) + ' ' + RST);
    wr(' ' + BUCKETS[process.stdout.rows - y - 1]);
    ii = (ii + 1) % RNG;
  }
  i = (i + 1) % RNG;
*/
}

function emptyBuckets()
{
  var x = [];
  while (x.length < BUCKETS.length)
    x.push(0);
  return x;
}

function findBucket(val)
{
  var bucket = BUCKETS.length - 1;
  while (bucket > 0) {
    if (val >= BUCKETS[bucket])
      return bucket;
    bucket--;
  }
  return 0;
}

var proc = spawn('ssh', ['alpha', 'mpstat', '1']);
proc.stdout.on('data', function(data) {
  var lines = data.toString().split('\n').map(function(l) {
    return l.trim().split(/\s+/);
  });
  var buckvals = emptyBuckets();
  for (var q = 1; q < lines.length; q++) {
    var ll = lines[q];
    if (ll.length === 16) {
      var usage = 100 - Number(ll[15]);
      var bucket = findBucket(usage);
      buckvals[bucket]++;
    }
  }
  newRow(buckvals);
});
proc.on('exit', function() {
  process.exit(0);
});


//wr(CSI + '8;1H');
