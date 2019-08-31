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

The first lookup table adds 16KB of mostly redundant data, but should make for fast access and be acceptable overhead for any mcu that can already take 80KB. Teensy 4 has plenty

"""
import struct
import sys

class Sequence():

    def __init__(self, taps, params, norms):
        self.taps = taps
        self.params = [x/y for x,y in zip(params,norms)]


    def __repr__(self):
        return f'{self.taps}, {self.params}'

class SeqTable():

    def __init__(self, filename):
        self.load(filename)


    def load(self, filename):
        self.infile = filename
        raw = open(filename, 'rb').read()
        
        self.data = {} #lengths to sequences
        
        while True:
            if len(raw) == 0: break
            length, = struct.unpack('H', raw[:2]); raw=raw[2:]
            seq_count, = struct.unpack('H', raw[:2]); raw=raw[2:]
            param_norms, = struct.unpack('H', raw[:2]); raw=raw[2:]
            param_norms = (param_norms, 4096)
            self.data[length] = tdata = []
            print(f'{length} {seq_count} {param_norms}')

            for seq_index in range(seq_count):
                ttaps, = struct.unpack('H', raw[:2]); raw=raw[2:]
                tparams = struct.unpack('HH', raw[:4]); raw=raw[4:]
                tseq = Sequence(ttaps, tparams, param_norms)
                tdata.append(tseq)


if __name__ == '__main__':
    table = SeqTable("length_lookup_inv")

    print(f'{len(table.data.keys())}')
    print(table.data[11])

















