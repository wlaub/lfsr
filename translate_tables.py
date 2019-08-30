"""
Take an unsorted lfsr sequence length lookup table, sort it into an appropriate
direct lookup table, and generate the file.

Something like

Access vector table maps desired length to index of best available length
00000   lookup of length 0 index (4) [nearest to that length if none]
...     ...
16380   lookup of length 4096 index (4)
16384   [next section]

Data table lists all taps for each sequence length and their parameters
00000   sequence count for first length = N (2)
00002   center parameter normalization value (2)
00004   last parameter normalization value (2)
00006   taps 0 (2)
00008   center parameter 0 (2)
00010   last parameter 0 (2)
00012   taps 1 (2)
00014   center parameter 1 (2)
00016   last parameter 1 (2)
...     ...
N*6+0   taps N (2)
N*6+2   center parameter N (2)
N*6+4   last parameter N (2)

Maybee the parameters can be sort of soft-normalized to a knob so that
for example consider a knob from -1 to 1 and the values it maps to are:
-1 0 0 0 1
 a b c d e
This should go something like
-1 to -.33 = a
-.33 to -.11 = b
-.11 to .11 = c
.11 to .33 = d
 .33 to 1 = e
yielding the actual mapping of
-1 -.33 -.11 .11 .33
 a    b    c   d   e
The normalized values give the left edge so that when the value of the knob
first exceeds the parameter, that tap configuration is selected

So the sequence goes:
1. Find starting index
2. Get normalizes parameter thresholds
3. start stepping through sequences until center param is less than center threshold
4. continue stepping until last param is less than lest threshold 
    the data should be constructed such that it is impossible to not stop before crossing to the next center param value
5. That is your taps value

This sort of procedure is always going to be required to map a param value 0-4096 into a nearest discrete result. The first parameter can probably afford a lookup vector table, but you would need the same size table for every sublist, and that gets out of hand fast.

"""

