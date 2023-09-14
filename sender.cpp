#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <pthread.h>
#include <mutex>
#include <fcntl.h>


#include "verification.h"

using namespace std;

static int PAYLOAD_SIZE = 140;
static  int NUM_THREADS = 1;
static int backlog = 10;

struct sockaddr_in receiver;
int send_sock, recv_sock, num_packets;
int * pay_trans_state;
char ** payloads;
mutex * mtx;
pthread_t *send_threads, *recv_threads;

void readFile(string filepath, char ** buffer)
{
    int file_size;

    ifstream file (filepath);
    if (file.is_open()) {
        file.seekg(0, file.end);
        file_size = file.tellg();
        file.seekg(0, file.beg);
        *buffer = (char*) malloc(sizeof(char) * file_size);
        file.read (*buffer, file_size);
        file.close();
    }
    else {
        printf("Could not open %s", filepath.c_str());
        exit(0);
    }
}

void createPacketPayloads(char * buffer) {
    num_packets = ceil((double)strlen(buffer)/PAYLOAD_SIZE);

    payloads = new char *[num_packets];
    pay_trans_state = new int [num_packets];
    
    for(int i = 0; i < num_packets; i++) {
        payloads[i] = new char [PAYLOAD_SIZE];
        for(int j = 0; j < PAYLOAD_SIZE; j++) {
            payloads[i][j] = buffer[i*PAYLOAD_SIZE + j];
        }
        // printf("Packet #%d: %s\n", i, payloads[i]);
    }
}

void sendPacket()
{
    int n;
    int length = sizeof(struct sockaddr_in);

    for(int i = 0; i < num_packets; i++)
    {
        n = sendto(send_sock, payloads[i], strlen(payloads[i]), 0, (const struct sockaddr *)&receiver,length);
        if (n < 0) {
            printf("Unable to use sendto");
            exit(0);
        }
    }
}

void * receiveACK(void * arg)
{
    char * data;
    unsigned int length;
    int n;

    n=read(send_sock, data, 256);
    if (n < 0) {
        printf("Unable to use recvfrom");
        exit(0);
    }
    
    printf("%s", data);
    if(data != NULL) {
        break;
    }
        
    return 0;
}

int main(int argc, char *argv[])
{
    string filepath;
    int receiver_port, j;
    struct hostent *receiver_ip;
    char * buffer;

    if (argc != 3) {
        fprintf(stderr, "ERROR, missing information. Please use the following format.\n \
./custom_ftp <sending_port> <local_filepath_to_transfer>\n");
        exit(0);
    }
    
    //Parse command line arguments
    receiver_port = atoi(argv[2]);
    filepath = argv[3];
    
    readFile(filepath, &buffer);
    createPacketPayloads(buffer);
    pay_trans_state = new int[num_packets];
    mtx = new mutex[num_packets];

    //Create UDP sockets
    send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) {
        printf("Unable to create UDP socket for sending file");
        exit(0);
    }

    recv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (recv_sock < 0) {
        printf("Unable to create UDP socket for receiving ACKs");
        exit(0);
    }

    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(receiver_port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
    
    sendPacket();
    
    close(send_sock);
    close(recv_sock);
    return 0;
}
