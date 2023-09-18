#include <stdio.h>

using namespace std;

static int BLOCK_SIZE = 2; // measured in bytes

int createChecksum(char * text)
{
    unsigned int sum, mask;

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
                // printf("%c    %x    %x\n", text[i*BLOCK_SIZE+j], (text[i*BLOCK_SIZE+j] & 0xff), sum);
            }
        }
    }

    for(int x = 0; x < BLOCK_SIZE; x++) {
        mask += 0xff << (8*x);
    }

    while(sum >> (8*BLOCK_SIZE)) {
        sum = (sum & mask) + (sum >> (8*BLOCK_SIZE));
    }

    return (int)((~sum + 1) & mask);
}

bool verifyChecksum(char * text)
{
    int checksum, index;
    unsigned int sum;
    const char * array;
    string tmp = text;
    unsigned int mask = 0;
    int num_blocks = (int) strlen(text) / BLOCK_SIZE;
    if(strlen(text) % BLOCK_SIZE != 0) {
        num_blocks++;
    }

    index = tmp.find_first_of(':');
    try
    {
        checksum = stoi(tmp.substr(0,index));
        array = tmp.substr(index).c_str();
    }
    catch(const std::exception& e)
    {
        return false;
    }
    
    sum = (unsigned int) checksum;

    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            if(strlen(array) > i * BLOCK_SIZE + j)
            {
                sum += ((array[i*BLOCK_SIZE+j] & 0xff) << (8*(BLOCK_SIZE-1-j)));
            }
        }
    }

    for(int x = 0; x < BLOCK_SIZE; x++) {
        mask += 0xff << (8*x);
    }

    while(sum >> (8*BLOCK_SIZE)) {
        sum = (sum & mask) + (sum >> (8*BLOCK_SIZE));
    }

    if ((sum & mask) == 1) {
        return true;
    }

    return false;
}