#include <stdio.h>

using namespace std;

static int BLOCK_SIZE = 2; // measured in bytes

void createChecksum(char * text,  unsigned char * checksum)
{
    unsigned int sum, mask, checksum_full;
    int total_bytes;
    char block [BLOCK_SIZE * 8];

    int num_blocks = (int) strlen(text) / BLOCK_SIZE;
    if(strlen(text) % BLOCK_SIZE != 0) {
        num_blocks++;
    }
    // printf("NUM: %d, text: %s\n", strlen(text), text);

    sum = 0;
    mask = 0;
    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            if(strlen(text) > i * BLOCK_SIZE + j)
            {
                sum += ((text[i*BLOCK_SIZE+j] & 0x00ff) << (8*(BLOCK_SIZE-1-j)));
                printf("%c    %x    %x\n", text[i*BLOCK_SIZE+j], (text[i*BLOCK_SIZE+j] & 0xff), sum);
            }
        }
    }

    for(int x = 0; x < BLOCK_SIZE; x++) {
        mask += 0xff << (8*x);
    }

    while(sum >> (8*BLOCK_SIZE)) {
        sum = (sum & mask) + (sum >> (8*BLOCK_SIZE));
    }

    printf("Final sum%x\n", sum);
    checksum_full =  (~sum + 1) & mask;
    printf("Full:%x\n", checksum_full);

    for(int y = 0; y < BLOCK_SIZE ; y++) {
        checksum[y] = 0;
        checksum[y] = (unsigned char) (checksum_full >> (8*(BLOCK_SIZE-1-y))) & 0x00ff;
    }
}

bool verifyChecksum(char * text)
{
    unsigned int mask = 0;
    unsigned int sum = 0;
    int num_blocks = (int) strlen(text) / BLOCK_SIZE;
    if(strlen(text) % BLOCK_SIZE != 0) {
        num_blocks++;
    }

    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            if(strlen(text) > i * BLOCK_SIZE + j)
            {
                sum += ((text[i*BLOCK_SIZE+j] & 0xff) << (8*(BLOCK_SIZE-1-j)));
                printf("%c    %d    %x\n", text[i*BLOCK_SIZE+j], 8*(BLOCK_SIZE-1-j), sum);
            }
        }
    }

    for(int x = 0; x < BLOCK_SIZE; x++) {
        mask += 0xff << (8*x);
    }

    while(sum >> (8*BLOCK_SIZE)) {
        sum = (sum & mask) + (sum >> (8*BLOCK_SIZE));
    }

    printf("Final sum%x", sum);
    if ((sum & mask) == 1) {
        return true;
    }

    return false;
}