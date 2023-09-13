#include <stdio.h>

using namespace std;

static int SEQ_BITS = 32;
static int BLOCK_SIZE = 2; // measured in bytes

short createChecksum()//char * text)
{
    uint sum;
    int total_bytes, checksum;
    char * text = "hello";
    char block [BLOCK_SIZE * 8];

    int num_blocks = sizeof(text) / BLOCK_SIZE;
    if(sizeof(text) % BLOCK_SIZE != 0) {
        num_blocks++;
    }

    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            if(sizeof(text) > i * BLOCK_SIZE + j)
            {
                sum += (text[i*BLOCK_SIZE+j] << 8*j);
            }
        }
    }
    printf("%d", checksum);

    // return checksum;
    return 0;
}

bool verifyChecksum(char * text) {
    return true;
}

void combinePacketInfo(char * payload, int seq_num)
{
    
}
