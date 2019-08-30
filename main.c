#include <stdio.h>
#include <math.h>

//#define N 2048
#define N 4096
#define BUFL 4096
#define BUFMASK 0xfff

#define MAIN_SEQ 0x10000
#define SEQ_MASK (int)(0x0ffff)

#define INVERT 1

#define MAX_SEQS 100000

unsigned int get_mask(unsigned int taps)
{
    unsigned int result = taps;
    while(result != (result | (result >> 1)))
    {
        result |= (result>>1);
    }
    return result;
}

unsigned int step_lfsr(unsigned int taps, unsigned int state, unsigned int mask)
{
    unsigned int result;
    unsigned int fb = (__builtin_popcount(state&taps)+INVERT)%2;
    if(INVERT == 0)
        if(state == 0) fb = 1;
    result = (state<<1)|fb;
    result &= mask;
    return result;
}

unsigned int lock_seqs(unsigned int* data, unsigned short sequence, unsigned int mask)
{
    unsigned int result = 0;
    for(unsigned int i = 0; i <= mask; ++i)
    {
        if((data[i] &SEQ_MASK) == 1)
        {
            result += 1;

            data[i] = ((data[i]&(~SEQ_MASK)) | (sequence&SEQ_MASK));
        }
    }
    return result;
}

unsigned int get_next_empty(unsigned int* data, unsigned int mask)
{   //This assumes you've already done 0
    for(unsigned int i = 0; i <= mask; ++i)
    {
        if(data[i] == 0)
        {
            return i;
        }
    }
    return 0; // there isn't one
}

typedef struct
{
    unsigned short taps;
    unsigned short index;
    unsigned short length;
    unsigned short chance;
    unsigned short diversity;
    unsigned short values[BUFL];
} 
Sequence;

unsigned int find_last_sequence(Sequence** buffer, unsigned int length)
{
//Find the index of the first available slot in the buffer for sequences of
//the given length.
    unsigned int index = 0;
    while(buffer[length*BUFL+index] != 0)
    {
        index +=1;
    }
    return index;
}

void print_sequence(Sequence* sequence, int forceShow)
{
        if(sequence->index == 2 || forceShow)
        {
            printf("%04x ",
                sequence->taps
                );

        }
        else
        {
            printf("     ");
        }

        printf("%04i % 7.2f%% %04x ",
            sequence->diversity,
            100.0*sequence->chance/BUFL,
            sequence->length
            );
       
        for(int j = 0; j < sequence->length && j < 20; ++j)
        {
            if((sequence->values[j]&1) == 0)
            {
                printf("_");
            }
            else
            {
                printf("-");
            }
        }

        printf("\n");

}

int main()
{

    printf("Lettuce, begins\n");
    unsigned int* seqs[N];
    unsigned int lock_counts[2];

    Sequence* sequences;
    sequences = malloc(sizeof(Sequence)*MAX_SEQS);
    memset(sequences, 0, sizeof(Sequence)*MAX_SEQS);

    Sequence** lengthLookup; //Sequence length->sequences
    lengthLookup = malloc(sizeof(Sequence*)*BUFL*BUFL);
    memset(lengthLookup, 0, sizeof(Sequence*)*BUFL*BUFL);

    for(unsigned int i = 0; i < N; ++i)
    {
//        seqs[i] = malloc(2*N);
        seqs[i] = malloc(BUFL*4);
    }

    // seqs [mask][val] represents the sequence index of val under mask
    // sequence index is of the form 0xABBB
    // A - sequence flags
    // B - sequence id
    // special sequences ids:
    // 0 - No sequence
    // 1 - Current sequences
    // sequence flags:
    // 0 - transient segment
    // 1 - looping segment

/* TODO:
 * For each taps:
 *      until you run out of sequences:
 *          Count their periods and transient lengths
 *          Generate a block of data that goes
 *          SEQUENCE LENGTH | TAPS | SEQUENCE VALUES
 *          with length 2 + 2 + 2*(SEQUENCE LENGTH) bytes
 *          Store these in a sequences array - find rule for number of sequences
 *          On a sequence collision, update transient length for the existing sequence
 * */



    unsigned int taps = 0;
    unsigned int mask;
    int done = 0;
    unsigned short sequence;
    unsigned int value;
    unsigned int next;

    unsigned int seqBufferIndex = 0;

    unsigned int seq_count = 0;
    unsigned int most_high=0;
    unsigned int most_high_taps = 0;

    unsigned int max_mask = get_mask(N-1);
    max_mask=BUFMASK;

    unsigned int masterSeqCount[BUFL*sizeof(unsigned int)];
    memset(masterSeqCount, 0, BUFL*sizeof(unsigned int));

    unsigned int tapIndexStart = 0; //Where the sequences for the current taps start
    //For the purpose of post-analysis characterization

    for (taps = 1; taps < N; ++taps)
    {
        printf("Analyzying taps %x\n", taps);
        memset(seqs[taps], 0, BUFL*4);
        mask = get_mask(taps);
        done = 0;
        sequence = 2;
        value = 0;
        seqs[taps][value] = 1;
        printf("Starting search\n");
        max_mask = mask;

        tapIndexStart = seq_count;

        while(done == 0)
        {
            next = step_lfsr(taps, value, max_mask);
            unsigned int reg = seqs[taps][next];
//            printf("%x/%x ", next, reg);
            if(reg != 0)
            {
                if((reg&SEQ_MASK) != 1) //We hit a previous sequence
                {
                    printf("This shouldn't have happened. Figure out what went wrong.");
                    return 42;
                    /*
                    //go through and flag existing 1-flags as the found sequence
                    if(seqs[taps][value] != 0)
                    {
                        printf("!!!!!!!!!!!");
                        return 43;
                    }

                    seqs[taps][value]=1;
                    lock_seqs(seqs[taps], reg&SEQ_MASK, max_mask);
                    for(unsigned int i = 0; i < seq_count; ++i)
                    {
                        if(sequences[i].taps == taps 
                            && sequences[i].index == reg&SEQ_MASK)
                        {
                            sequences[i].transients += lock_counts[1]; 
                        }
                    }

                    next = get_next_empty(seqs[taps], max_mask);
                    printf("- Found existing sequence. Next: %x\n%x ", next, taps); 
                    if(next == 0)
                    {
                        done = 1;
                    }
                    else
                    {
                        seqs[taps][next] = 1;
                    }
                    */
                }
                else if((reg&MAIN_SEQ) == 0) //We are now looping
                {
                    seqs[taps][next] |= MAIN_SEQ;
                    sequences[seq_count].values[seqBufferIndex] = next;
                    ++seqBufferIndex;
                }
                else //Finished looping
                {
                    seqs[taps][value] |= MAIN_SEQ;
                    sequences[seq_count].values[seqBufferIndex] = value;

                    lock_seqs(seqs[taps], sequence, max_mask);

                    if (sequence > most_high) 
                    {
                       most_high = sequence;
                       most_high_taps = taps;
                    }

                    unsigned short length = seqBufferIndex;

                    sequences[seq_count].taps = taps;
                    sequences[seq_count].index = sequence;
                    sequences[seq_count].length = length;

                    masterSeqCount[length] += 1;
                    unsigned int lookupIndex = find_last_sequence(lengthLookup, length);
                    if(lookupIndex == 0 || (lookupIndex > 0 && lengthLookup[length*BUFL+lookupIndex-1]->taps != taps))
                    {
                        lengthLookup[length*BUFL+lookupIndex] = &sequences[seq_count];
                    }

                    sequence += 1;
                    seq_count +=1;
                    next = get_next_empty(seqs[taps], max_mask);
//                    printf("- Finished. Next: %x\n%x ", next, taps);
//                    seqBufferIndex is sequence length
//                    seqBuffer is sequence
//                  TODO: Extract finished sequence data here.

                    if(next == 0)
                    {
                        done = 1;

                    }
                    else
                    {
                        seqs[taps][next] = 1;
                    }
                    seqBufferIndex=0;
                }
            }
            else
            {
                seqs[taps][next] = 1;
            }
            value = next;

        }

        unsigned int len_counts[BUFL*sizeof(unsigned int)];
        memset(len_counts,0,BUFL*sizeof(unsigned int));
        for(unsigned int i = tapIndexStart; i < seq_count; ++i)
        {
            len_counts[sequences[i].length]+=1;
        }
        for(unsigned int i = tapIndexStart; i < seq_count; ++i)
        {
            sequences[i].diversity = len_counts[sequences[i].length];
            sequences[i].chance =
               len_counts[sequences[i].length] * sequences[i].length*BUFL/(max_mask+1);
        }

    }


    unsigned int maxLen = 0;
    unsigned int maxLenAmt = 0;
    unsigned int lenCount = 0;
    for(int i = 0; i < BUFL; ++i)
    {
        if(masterSeqCount[i] > maxLenAmt)
        {
            maxLenAmt = masterSeqCount[i];
            maxLen = i;
        }
        if(masterSeqCount[i] > 0)
        {
            lenCount += 1;
            printf("%4ix%4i\n", i, masterSeqCount[i]);
        }
    }
    printf("Most common length: %i appears %i times.\n", maxLen, maxLenAmt);
    printf("%i different sequence lengths\n", lenCount);

    unsigned int entryCount = 0;

    FILE* fp = fopen("length_lookup", "wb");
    //Format:
    //0     length 0 (2)
    //2     sequence count = N (2)
    //4     maxdiversity (2)
    //6     sequence 0 (6)
    //  6       taps (2)
    //  8       diversity (2)
    //  10      chance (2)
    //12    sequence 1 (6)
    //      ...
    //6+6*N length 1 (2)

    printf("taps dvsty  chnce  lgth seq\n");       
    for(unsigned int i = 0; i < BUFL; ++i)
    {
        unsigned short maxDiversity = 0;
        unsigned int j = 0;
        for(j = i*BUFL; lengthLookup[j]!= 0; ++j)
        {
            entryCount += 1;
            if(lengthLookup[j]->diversity > maxDiversity)
            {
                maxDiversity = lengthLookup[j]->diversity;
            }
            if(i == 11)
            {
                print_sequence(lengthLookup[j], 1);
            }
        }
        unsigned short sequenceCount = j-i*BUFL;
        //write header
        if(sequenceCount > 0)
        {
            fwrite(&sequenceCount, sizeof(unsigned short), 1, fp);
            fwrite(&maxDiversity, sizeof(unsigned short), 1, fp); 
            for(unsigned int j = i*BUFL; lengthLookup[j]!= 0; ++j)
            {
                fwrite(&(lengthLookup[j]->taps), sizeof(unsigned short), 1, fp);
                fwrite(&(lengthLookup[j]->diversity), sizeof(unsigned short), 1, fp);
                fwrite(&(lengthLookup[j]->chance), sizeof(unsigned short), 1, fp);
                //write data
            }
        }

    }
    fclose(fp);
    printf("Found %i total lookup table entries = %i B\n", entryCount, entryCount*6);

    printf("taps dvsty  chnce  lgth seq\n");
    for(unsigned int i = 0; i < seq_count; ++i)
    {
        if(i < 32)
        {
            print_sequence(&sequences[i], 0);
        }
    }
/*
    for (taps = 1; taps < N; ++taps)
    {
        printf("%03x: ", taps);
        for(unsigned int j = 0; j <= max_mask; ++j)
        {
            unsigned int value = seqs[taps][j];
            if(value != 0)
            {
                if( (value&MAIN_SEQ) == 0)
                {
//                    printf("---- ");
                }
                else
                {
                printf("%03x-%04x ", j,value);
                }
            }
            else
            {
//                printf("XXXX ");
            }
        }

        printf("-|\n");

    }
*/
    printf("Found %i sequences in %i taps. Most high = %i as %x\n", seq_count, N, most_high, most_high_taps);

    return 0;
}
