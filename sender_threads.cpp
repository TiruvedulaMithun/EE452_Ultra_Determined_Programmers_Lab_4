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
#include <arpa/inet.h>
#include <ctime>

#include "verification.h"
#include "packet.h"

using namespace std;

static  int NUM_THREADS = 1;
static int DUPLICATES = 2;
static int READ_LIMIT = 10;
static int backlog = 10;

struct sockaddr_in client;
int send_sock, recv_sock, new_recv_sock, num_packets, file_size;
int * pay_trans_state;
char ** payloads;
mutex * mtx;
pthread_t *send_threads, *recv_threads;


void readFile(string filepath, char ** buffer)
{
    int file_size;

    ifstream file;
    file.open(filepath.c_str());
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
    num_packets = ceil((double)strlen(buffer)/LINE_SIZE);

    payloads = new char *[num_packets];
    
    for(int i = 0; i < num_packets; i++) {
        payloads[i] = new char [LINE_SIZE];
        for(int j = 0; j < LINE_SIZE; j++) {
            payloads[i][j] = buffer[i*LINE_SIZE + j];
        }
        // printf("Packet #%d: %s\n", i, payloads[i]);
    }
}

void formatPacket(Packet * packet) {
    int checksum;
    char * tmp = new char [LINE_SIZE];

    strcat(tmp, ":");
    strcat(tmp, to_string(packet->packet_num).c_str());
    strcat(tmp, ";");
    strcat(tmp, packet->payload);
    checksum = createChecksum(tmp);

    strcpy(packet->data, to_string(checksum).c_str());
    strcat(packet->data, tmp);
}

void printTime(string phrase) {
    time_t ms = time(nullptr);
    printf("%s%ld\n",phrase.c_str(), ms);
}

void * sendPacket(void * arg)
{
    int n;
    int length = sizeof(struct sockaddr_in);
    Packet packet = Packet();

    bool init = true;
    int acks = 0;
    int num_p = num_packets;
    

    strcpy(packet.payload, to_string(num_packets).c_str());
    packet.packet_num = 0;
    formatPacket(&packet);

    printTime("File Transfer Start Time (ms):");

    while(init) {
        n = sendto(send_sock, packet.data, sizeof(packet.data), 0, (const struct sockaddr *)&client,length);
        mtx[0].lock();
        init = (pay_trans_state[0] != 2);
        mtx[0].unlock();
    }

    while(acks <= num_p) {
        acks = 0;
        for(int i = 1; i <= num_p; i++)
        {
            mtx[i].lock();
            if(pay_trans_state[i] != 2) {
                strcpy(packet.payload, payloads[i-1]);
                packet.packet_num = i;
                memset(packet.data, '\0', sizeof(packet.data));
                formatPacket(&packet);
                for(int j = 0; j < DUPLICATES; j++) {
                    n = sendto(send_sock, packet.data, sizeof(packet.data), 0, (const struct sockaddr *)&client,length);
                    if (n < 0) {
                        printf("Unable to use sendto");
                        exit(0);
                    }
                }
                pay_trans_state[i] = 1;
            }
            else {
                acks++;
            }
            mtx[i].unlock();
        }
    }
    
    return NULL;
}

void * receiveACK(void * arg)
{
    char data[50];
    int n, acks, packet_num;
    string tmp;

    int length = 0;
    acks = 0;

    while(1)
    {
        memset(data, '\0', sizeof(data));
        n = read(new_recv_sock, data, sizeof(data));//, 0, (struct sockaddr *)&cli_addr, &length);
        tmp = data;
        
        if (strlen(data) > 4){
            if (tmp.substr(0, 4) == "ACK "){
                packet_num = stoi(tmp.substr(4));
                
                mtx[packet_num].lock();
                if (pay_trans_state[packet_num] != 2) {
                    acks++;
                }
                pay_trans_state[packet_num] = 2;
                mtx[packet_num].unlock();
                if(acks > num_packets) {
                    printTime("File Transfer End Time (ms):");
                    // close(new_recv_sock);
                    // close(send_sock);
                    // close(recv_sock);
                    exit(0);
                }
            }
        }
        else if(tmp != "") {
            printf("The data received did not fit the ack format: %s\n", data);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    string filepath, tmp;
    int udp_port, tcp_port, j, offset;
    struct hostent *cli_ip;
    char * buffer, * clientIP, data [256];
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    bool start = false;

    if (argc != 5) {
        fprintf(stderr, "ERROR, missing information. Please use the following format.\n \
./custom_ftp_sender <client_ip> <udp_port_num> <tcp_port_num> <local_filepath_to_transfer>\n");
        exit(0);
    }
    
    //Parse command line arguments
    clientIP = argv[1];
    udp_port = atoi(argv[2]);
    tcp_port = atoi(argv[3]);
    filepath = argv[4];

    readFile(filepath, &buffer);
    createPacketPayloads(buffer);
    printf("Num_Pack:%d\n", num_packets);
    pay_trans_state = new int[num_packets + 1];
    mtx = new mutex[num_packets + 1];

    send_threads = new pthread_t[NUM_THREADS];
    recv_threads = new pthread_t[NUM_THREADS];

    recv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (recv_sock < 0) {
        printf("Unable to create UDP socket for receiving ACKs");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(tcp_port);

    if (bind(recv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        printf("ERROR on binding");
    listen(recv_sock,5);
    clilen = sizeof(cli_addr);
    new_recv_sock = accept(recv_sock, (struct sockaddr *) &cli_addr, &clilen);
    if (new_recv_sock < 0) 
        printf("ERROR on accept");

    // if (inet_ntop(AF_INET, &(cli_addr.sin_addr), clientIP, INET_ADDRSTRLEN) == NULL) {
    //     perror("Converting IP address failed");
    //     return 1;
    // }

    cli_ip = gethostbyname(clientIP);
        
    //Create UDP sockets
    send_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sock < 0) {
        printf("Unable to create UDP socket for sending file");
        exit(0);
    }
    client.sin_family = AF_INET;
    bcopy((char *)cli_ip->h_addr, (char *)&client.sin_addr, cli_ip->h_length);
    client.sin_port = htons(udp_port);
    
    while(!start) {
        j = read(new_recv_sock, data, 256);//, 0, (struct sockaddr *)&cli_addr, &length);
        tmp = data;
            
        if(tmp.substr(0, 5) == "Start") {
            start = true;
        }
    }
    
    for (int i = 0; i  < NUM_THREADS; ++i) {
    	j = 0;
        j = pthread_create(&recv_threads[i], NULL, &receiveACK, (void*) &udp_port);
        if (j) {
            printf("A request can't be procceses.\n");
        }

        j = pthread_create(&send_threads[i], NULL, &sendPacket, (void*) &udp_port);
        if (j) {
            printf("A request can't be procceses.\n");
        }
        pthread_join(send_threads[i], NULL);
        pthread_join(recv_threads[i], NULL);
    }
    
    return 0;
}
