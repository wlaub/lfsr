#include <stdio.h>
#include <math.h>

//#define N 2048
#define N 4096
#define BUFL 4096
#define BUFMASK 0xfff

#define MAIN_SEQ 0x10000
#define SEQ_MASK (int)(0x0ffff)

#define INVERT 0

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

void lock_seqs(unsigned int* data, unsigned short sequence, unsigned int mask, unsigned int* counts)
{
    counts[0] = 0;
    counts[1] = 0;
    for(unsigned int i = 0; i <= mask; ++i)
    {
        if((data[i] &SEQ_MASK) == 1)
        {
            if((data[i]&MAIN_SEQ) == 0)//Then transient
            {
                counts[1] +=1;
            }
            else
            {
                counts[0] +=1;
            }

            data[i] = ((data[i]&(~SEQ_MASK)) | (sequence&SEQ_MASK));
        }
    }
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
    unsigned short transients;
    unsigned short length;
    unsigned short values[BUFL];
} 
Sequence;

int main()
{

    printf("Lettuce, begins\n");
    unsigned int* seqs[N];
    unsigned int lock_counts[2];

    Sequence* sequences;
    sequences = malloc(sizeof(Sequence)*MAX_SEQS);
    memset(sequences, 0, sizeof(Sequence)*MAX_SEQS);

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
        while(done == 0)
        {
            next = step_lfsr(taps, value, max_mask);
            unsigned int reg = seqs[taps][next];
//            printf("%x/%x ", next, reg);
            if(reg != 0)
            {
                if((reg&SEQ_MASK) != 1) //We hit a previous sequence
                {
                    //go through and flag existing 1-flags as the found sequence
                    if(seqs[taps][value] != 0)
                    {
                        printf("!!!!!!!!!!!");
                        return 43;
                    }

                    seqs[taps][value]=1;
                    lock_seqs(seqs[taps], reg&SEQ_MASK, max_mask, lock_counts);
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
                    ++seqBufferIndex;

                    lock_seqs(seqs[taps], sequence, max_mask, lock_counts);

                    if (sequence > most_high) 
                    {
                        most_high = sequence;
                        most_high_taps = taps;
                    }

                    unsigned short length = seqBufferIndex;

                    sequences[seq_count].taps = taps;
                    sequences[seq_count].index = sequence;
                    sequences[seq_count].length = lock_counts[0];
                    sequences[seq_count].transients = lock_counts[1];

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
    }


    printf("taps trns lgth\n");
    for(unsigned int i = 0; i < seq_count; ++i)
    {
        if(sequences[i].transients != 0)
        {
            if(sequences[i].index == 2)
            {
            printf("%04x ",
                sequences[i].taps
                );
 
            }
            else
            {
                printf("     ");
            }

            printf("%04i %04i\n",
                sequences[i].transients,
                sequences[i].length
                );
            
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
