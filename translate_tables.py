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
import pprint
from collections import OrderedDict

def spread_equals(count, fill_range):
    """
    Fill the specified range with N spaces to return a list of the form
    [fill_range[0], fill_range[0]+width/count]
    Returns the right edge of the interval
    """
    width = fill_range[1]-fill_range[0]
    a = fill_range[0]
    result = [a+(i+1)*width/count for i in range(count)]
    return result

def spread_values(data : OrderedDict)->OrderedDict:
    """
    Data is of the form {value:count}
    result is of the form {old_value:[new_value,...]}
    Such that new_value is what the knob must be greater than to select the
    corresponding option from old_value
    for example:
    The input dictionary
    {
    0  :1,
    .5 :3,
    1  :1,
    }
    indicates that there are values at knob positions 0, .5, and 1
    (left, center, right), and that there is 1 value at left and right
    and 3 values at center.
    From 0 to 0.25, the knob should select the value for left.
    From .25 to .75, the knob should select the value for center.
        Since there are 3 values, they should be spread equally within the
        interval: .25 -> .42 -> .58 -> .75
    From .75 to 1, the knob should select the value for right.
    The result dictionary is then
    {
    0 : [0],
    .5: [.25,.42,.58],
    1 : [.75],
    }

   
    """
    old_values = list(data.keys())

    #new values maps old_values to a range [left, right]
    new_values = [(old_values[0],[0])]
    for prev_val, next_val in zip(old_values[:-1], old_values[1:]):
        new_edge = (prev_val+next_val)/2
        new_values[-1][1].append(new_edge)
        new_values.append((next_val, [new_edge]))

    new_values[-1][1].append(1)

    result = OrderedDict(new_values)

    for old_val, count in data.items():
        new_val = result[old_val]
        fill_range = list(new_val)
        new_val.clear()
        new_val.extend(spread_equals(count, fill_range))
        pass

    return result

results = spread_values(OrderedDict([(0,1),(.5,3),(1,1)]))
pprint.pprint(results)


def generate_index(sequences, depth_index, max_depth, reverse = False):
    """
    Recursively generate a searchable index of data
    """
    

    param_values = []
    param_counts = []
    for seq in sequences:
        taps = seq.taps
        params = seq.params
        if reverse: params = params[::-1]
        if not params[depth_index] in param_values:
            param_values.append(params[depth_index])
            param_counts.append(1)
        else:
            param_counts[-1]+=1

    param_values = list(zip(param_values, param_counts))

    number = len(param_values)
    terminal = depth_index >= max_depth-1

    if not terminal:
        spread_map = spread_values(OrderedDict([(x[0],1) for x in param_values]))
    else:
        spread_map = spread_values(OrderedDict(param_values))

    result = []
    if terminal:
        access_value = -1
        result.append(0)
        result.append(len(sequences))
        #0 | short
        #len(sequences) | short
        for seq in sequences:
            taps = seq.taps
            params = seq.params
            if reverse: params = params[::-1]
            #Generate index on the segment of sequences:
            #mapping spread_map index to taps value
            #compute length of resulting chunk
            #return the resulting chunk
            #at this point, the params[-1] will be a key mapping
            #to the spread_values lists, so
            #keep track of index and prev_value
            #
            #
            if access_value != params[-1]:
                index = 0
                access_value = params[-1]

            #data = list of 
            #taps | short
            #key | short
            key = int(spread_map[access_value][index]*(2**16-1))
            result.append(key)
            result.append(taps)

            index +=1

        return result
    else:
        result.append(0)
        result.append(len(param_values))
        data = []
        # 0 | short
        # len(param_values) | short
        index = 0
        offset = len(param_values)*4+4 #TODO: Verify this
        for value, count in param_values:
            data_chunk = generate_index(sequences[index:index+count], depth_index+1, max_depth)
            #header append:
            #spread_map[value] | short
            #offset | short
            #
            result.append(int(spread_map[value][0]*(2**16-1)))
            result.append(offset)
            data.extend(data_chunk)
#            print(f'data: {data_chunk}')
            #datamap append: data
            index += count
            offset+=len(data_chunk)*2
 

            #Generate index over param_values pointing
            #to indexes on sequences[idx:idx+count] for depth_index+1
            #append subindex chunks into a list, then append that list
            #to the the master index of this
        #append index and data, return
#        print(f'result: {result}')
        result.extend(data)
        return result



class Sequence():

    def __init__(self, taps, params, norms):
        self.taps = taps
        self.params = params
        self.params = [x/y for x,y in zip(params,norms)]


    def _sort_key(self, reverse=False):
        """
        Return sorting tuple for this sequence. Reverse controls the
        sorting order of the params
        """
        if not reverse:
            return (*self.params, self.taps)
        else:
            return (*self.params[::-1], self.taps)

    def __repr__(self):
        return f'{self.taps}, {self.params}'

class SeqTable():

    def __init__(self, filename):
        self.master_index = None
        self.data_index = None
 
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


    def get_expanded_length_lookup(self):
        """
        Return an array mapping every possible sequence length to the best-fit
        available sequence length using spread_values.

        expanded_table is a list with at 4096 indices giving the best available
        sequence length for the desired value. It will be used to populate the
        sequence vector table
        """
        lengths = sorted(list(self.data.keys()))

        length_map = spread_values(OrderedDict([(x,1) for x in lengths]))

        length_map_items = list(length_map.items())
        expanded_table = []
        length_index = 0
        for index in range(4096):
            if index >= length_map_items[length_index][1][0]:
                length_index += 1
            expanded_table.append(length_map_items[length_index][0])

        return expanded_table

    def sort(self, reverse=False):
        """
        Go through and sort all the data
        """
        pass

    def spread(self, reverse=False):
        """
        Go through and spread all the parameters for each sequence to space
        them about their knobs on the range of 0 to 1. This will probably be
        scaled to 4096 to export, but that's a separate step.


        And go like param1 -> spread by counts of 1
        for each param 1:
                 param2 within param 1 -> spread by counts of taps

        """
        
        #this appears to be incorrect
        expanded_lookup = self.get_expanded_length_lookup()
        pprint.pprint(expanded_lookup[:10])

        lengths = sorted(list(self.data.keys()))
        result = []
        master_index = {}
        offset = 0
        for length in lengths:
            sequences = sorted(self.data[length], key=lambda x: x._sort_key(reverse))

            result.append(length)
            data_chunk = generate_index(sequences, 0, 2, reverse)
            result.extend(data_chunk)
            for desired, actual in enumerate(expanded_lookup):
                if actual == length:
                    master_index[desired] = offset
            offset+=len(data_chunk)*2+2

        self.master_index = master_index
        self.data_index = result
        return master_index, result

    def write_data(self, filename):
        """
        Write the master_index and result to a binary file
        """

        adds = list(self.master_index.values())
        #address should point to the 0 of the index
        #for the actual data package, 4*4096+1+lookup_value
        adds = [x+4*4096+2 for x in adds] 

        print(adds[:10])

        top = struct.pack('I'*len(self.master_index), *adds)
        bottom = struct.pack('H'*len(self.data_index), *self.data_index)

        with open(filename, 'wb') as f:
            f.write(top)
            f.write(bottom)


    def get_taps(self, length, params):
        """
        Find and return the taps value for the given sequence length
        and params
        """
        if self.master_index == None: return 0
        master_idx = self.master_index
        data = self.data_index

        address = int(master_idx[length]/2)+1
        print(f'Taps for {length} = {data[address-1]}')
        print(data[address:address+2])
        N = data[address+1]
        ileft = 0
        iright = N
        i = 0
        while True:
            data_range = data[address+i*2], data[address+2+i*2]
            if params[0] < data_range[0]:
                i = int((i+ileft)/2)
            elif params[0] >= data_range[1]:
                i = int((i+iright)/2)
            else:
                subaddress = int(data[address+3+i*2]/2)+address
                jleft = 0
                jright = data[subaddress+1]
                j = 0
                while True:
                    data_range = data[subaddress+j*2], data[subaddress+2+j*2]
                    if params[1] < data_range[0]:
                        j = int((j+jleft)/2)
                    elif params[1] >= data_range[1]:
                        j = int((j+jright)/2)
                    else:
                        taps = int(data[subaddress+3+j*2])
                        return taps


    def print_seqs(self, length):
        if self.master_index == None: return
        master_idx = self.master_index
        data = self.data_index

        address = int(master_idx[length]/2)+1
        print(f'Data for {length} = {data[address-1]}')
        print(data[address:address+2])
        for i in range(data[address+1]):
            subaddress = int(data[address+3+i*2]/2)+address
            data_range = data[address+i*2], data[address+2+i*2]
            print(f'{i}: {data_range[0]} to {data_range[1]}: {data[subaddress:subaddress+2]}')
            for j in range(data[subaddress+1]):
                data_range = data[subaddress+j*2], data[subaddress+2+j*2]
                taps = int(data[subaddress+3+j*2])
                print(f'  {j}: {data_range[0]} to {data_range[1]} : {taps}')
     

if __name__ == '__main__':
    table = SeqTable("length_lookup_inv")
    table.spread(reverse=True)
    table.write_data('lookup_inv_0.taps')
    table.spread(reverse=False)
    table.write_data('lookup_inv_1.taps')


    print(f'{len(table.data.keys())}')
    pprint.pprint(
            sorted(table.data[10], key=lambda x:x._sort_key())
            )

    master_idx, data = table.spread(reverse=True)
    print(data[:100])
    print(len(data))
    print(len(master_idx))
#    print(master_idx)

    length = 17
    table.print_seqs(length)    

    taps = table.get_taps(3333, [32767, 32767])
    print(taps)
    assert taps == 3072

    taps = table.get_taps(17, [32767, 32767])
    print(taps)
    assert taps == 983 

    print(max(master_idx.values()))




