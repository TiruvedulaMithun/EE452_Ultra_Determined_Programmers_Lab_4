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

// TODO remove unused globals
static int PAYLOAD_SIZE = 140;
static  int NUM_THREADS = 1;
static int backlog = 10;
static int RECIEVER = 10;
static int MAX_QUEUE_SIZE = 1000;
struct sockaddr_in receiver;
int send_sock, recv_sock, num_packets;
int * pay_trans_state;
char ** payloads;
mutex * mtx;
pthread_t *send_threads, *recv_threads;
int PACKET_SIZE = 1024;

// Define a structure for a thread's arguments
typedef struct {
    int id;
    int socket;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    Packet queue[MAX_QUEUE_SIZE];
    int queue_size;
    int queue_front;
    int queue_rear;
} ThreadData;

// Define a structure for a packet
typedef struct {
    char data[PACKET_SIZE];
    int packet_num;
} Packet;



void *receieveHandler(void *socket_desc, void *num_threads, void* filepath, void* udp_queues, void* line_size) {
    printf("RECIEVED MESSAGE FROM CLIENT!\n");  
    
    int client_socket = *(int *)socket_desc;

    // get message type from client
    char buffer[1024];
    int valread = read(client_socket, buffer, 1024);
    if (valread < 0) {
        perror("Error reading from socket");
        exit(EXIT_FAILURE);
    }

    // parse message type
    string message_type = buffer;
    // if message type starts with "get" then it is a get request
    if (message_type.substr(0, 4) == "init") {
        // TODO handle init request
        ThreadData *udp_queues = new ThreadData[num_threads];
        initRequestHandler(buffer, udp_queues);
    } else if (message_type.substr(0, 3) == "ack") {
        // TODO handle put request
        ackRequestHandler(buffer);
    } else  if (message_type.substr(0, 4) == "nack") {
        // TODO handle put request
        nackRequestHandler(buffer);
    }        
    
    close(client_socket);
    return NULL;
}

void *initRequestHandler(void * buffer, void * udp_queues){
    // initialize ack queue
    queue<Packet> ack_queue; //Malloc? TODO

    // parse list of open ports
    string ports = buffer;
    stringstream ss(buffer);
    string port;
    vector<int> open_ports;
    while (getline(ss, port, ',')) {
        open_ports.push_back(stoi(port));
    }

    // create udp sockets
    int udp_sockets[num_threads];
    for (int i = 0; i < num_threads; i++) {
        udp_sockets[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_sockets[i] < 0) {
            perror("Error creating socket");
            exit(EXIT_FAILURE);
        }
        udp_sockets[i].sin_family = AF_INET; // TODO check if this is correct
        udp_sockets[i].sin_addr.s_addr = INADDR_ANY; // TODO check if this is correct
        udp_sockets[i].sin_port = open_ports[i];

        if(bind(udp_sockets[i], (struct sockaddr *)&udp_sockets[i], sizeof(udp_sockets[i])) < 0) {
            perror("Error binding socket");
            exit(EXIT_FAILURE);
        }
    }

    // set up ThreadData
    for (int i = 0; i < num_threads; i++) {
        ThreadData udp_queue = udp_queues[i];
        udp_queue[i].id = i;
        udp_queue[i].socket = udp_sockets[i];
        udp_queue[i].queue_size = 0;
        udp_queue[i].queue_front = 0;
        udp_queue[i].queue_rear = 0;
        pthread_mutex_init(&udp_queue[i].mutex, NULL);
        pthread_cond_init(&udp_queue[i].cond, NULL);
    }

    // create udp threads
    pthread_t udp_threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        int j = pthread_create(&udp_threads[i], NULL, &workerThreadCreated, (void*) udp_queues[i], );
        if (j) {
            printf("A request can't be procceses.\n");
        }
    }
    
    // initialize loader thread to read file add to udp queue
    int j = 0;
    pthread_t loader_thread;
    j = pthread_create(&loader_thread, NULL, &loaderThread, (void*) filepath, (void*) udp_queues); //TODO match parameters 
    if (j) {
        printf("A request can't be procceses.\n");
    }
    return udp_queues;
}


void *workerThreadCreated(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int client_socket = data->socket;

    // bind socket to port
    if (bind(client_socket, (struct sockaddr *)&client_socket, sizeof(client_socket)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Wait for a packet to be available in the queue
        pthread_mutex_lock(&data->mutex);
        while (data->queue_size == 0) {
            pthread_cond_wait(&data->cond, &data->mutex);
        }

        // Dequeue a packet
        Packet packet = data->queue[data->queue_front];
        data->queue_front = (data->queue_front + 1) % MAX_QUEUE_SIZE;
        data->queue_size--;
        pthread_mutex_unlock(&data->mutex);

        // Send the packet to the client
        sendto(client_socket, packet.data, packet.length, 0, NULL, 0);
        // add packet to ack queue
        ack_queue.push(packet);
    }
}

void *loaderThread(void *arg, void *num_threads, void* filepath, void* line_size) {
    ThreadData *threads = (ThreadData *)arg;

    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int current_thread = 0;
    char line[line_size];

    while (fgets(line, sizeof(line), file) != NULL) {
        // Create a packet from the file data
        Packet packet;
        strncpy(packet.data, line, sizeof(packet.data));
        packet.length = strlen(packet.data);

        // Round-robin distribution of packets to worker threads
        pthread_mutex_lock(&threads[current_thread].mutex);
        while (threads[current_thread].queue_size == MAX_QUEUE_SIZE) {
            pthread_cond_wait(&threads[current_thread].cond, &threads[current_thread].mutex);
        }

        threads[current_thread].queue[threads[current_thread].queue_rear] = packet;
        threads[current_thread].queue_rear = (threads[current_thread].queue_rear + 1) % MAX_QUEUE_SIZE;
        threads[current_thread].queue_size++;

        // Signal the worker thread to process the packet
        pthread_cond_signal(&threads[current_thread].cond);
        pthread_mutex_unlock(&threads[current_thread].mutex);

        current_thread = (current_thread + 1) % num_threads;
    }

    fclose(file);
}

void *ackRequestHandler(void *buffer){
    // TODO parse buffer to get packet number
    int packet_num = 0;
    string rest_of_message = buffer.substr(4);
    stringstream ss(rest_of_message);
    ss >> packet_num;

    // TODO remove packet from ack queue
    while (ack_queue.front().packet_num != packet_num) {
        ack_queue.pop();
    }
}

void *nackRequestHandler(void *buffer){
    // TODO parse buffer to get packet number
    int packet_num = 0;
    string rest_of_message = buffer.substr(5);
    stringstream ss(rest_of_message);
    ss >> packet_num;

    // TODO add packet to 
    while (ack_queue.front().packet_num != packet_num) {
        ack_queue.pop();
    }
}


int main(int argc, char *argv[])
{
    printf("1!\n");   
    string filepath;
    char * buffer;
    ThreadData udp_queues[num_threads];


    if (argc != 3) {
        fprintf(stderr, "ERROR, missing information. Please use the following format.\n \
./custom_ftp <listen_port> <number_of_UDP_threads> <local_filepath_to_transfer>\n");
        exit(0);
    }
    
    //Parse command line arguments
    int listen_port = atoi(argv[1]);
    int num_udp_threads = atoi(argv[2]);
    filepath = argv[3];
    
    // check if file exists
    ifstream file(filepath);
    if (!file.good()) {
        fprintf(stderr, "ERROR, file does not exist.\n");
        exit(0);
    }

    // Open socket to listen for connections in new thread
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create a socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("2!\n");   
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = listen_port;

    // Bind the socket to the specified address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("3!\n");   
    // Listen for incoming connections
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);
    pthread_t thread;
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Create a thread to handle the client
        if (pthread_create(&thread, NULL, receieveHandler, (void *)&new_socket) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}
