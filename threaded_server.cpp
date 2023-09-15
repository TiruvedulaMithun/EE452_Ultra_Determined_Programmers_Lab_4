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
#include <queue>

#include "verification.h"
#include "packet.h"

using namespace std;

const static int MAX_QUEUE_SIZE = 1000;

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

static  int NUM_THREADS = 1;
static int RECIEVER = 10;
queue<Packet> ack_queue;

void initRequestHandler(string buffer, ThreadData * udp_queues, int num_threads){
    ack_queue = new queue<Packet>(); //TODO Malloc? (TM)

    // parse list of open ports
    string ports = buffer;
    stringstream ss(buffer);
    string port;
    vector<int> open_ports;
    while (getline(ss, port, ',')) {
        open_ports.push_back(stoi(port));
    }

    if (open_ports.size() < num_threads)
    {
        printf("Error expected more ports open on client");
        exit(EXIT_FAILURE);
    }

    // create udp sockets
    int udp_sockets[num_threads];
    for (int i = 0; i < num_threads; i++) {
        udp_sockets[i] = socket(AF_INET, SOCK_DGRAM, 0); //TODO give IP of client (TM)
        if (udp_sockets[i] < 0) {
            perror("Error creating socket");
            exit(EXIT_FAILURE);
        }
        udp_sockets[i].sin_family = AF_INET; // TODO check if this is correct (TM)
        udp_sockets[i].sin_addr.s_addr = INADDR_ANY; // TODO check if this is correct (TM)
        udp_sockets[i].sin_port = open_ports[i];

        // TODO might not have to do it here
        // if(bind(udp_sockets[i], (struct sockaddr *)&udp_sockets[i], sizeof(udp_sockets[i])) < 0) {
        //     perror("Error binding socket");
        //     exit(EXIT_FAILURE);
        // }
    }

    // set up ThreadData
    for (int i = 0; i < num_threads; i++) {
        udp_queues[i].id = i;
        udp_queues[i].socket = udp_sockets[i];
        udp_queues[i].queue_size = 0;
        udp_queues[i].queue_front = 0;
        udp_queues[i].queue_rear = 0;
        pthread_mutex_init(&udp_queues[i].mutex, NULL);
        pthread_cond_init(&udp_queues[i].cond, NULL);
    }

    // create udp threads
    pthread_t udp_threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        int j = pthread_create(&udp_threads[i], NULL, workerThreadCreated, (void*) &(udp_queues[i]));
        if (j) {
            printf("Worker thread could not be created.\n");
        }
    }
    
    // initialize loader thread to read file add to udp queue
    int j = 0;
    pthread_t loader_thread;
    j = pthread_create(&loader_thread, NULL, loaderThread, (void*) filepath, (void*) udp_queues); //TODO create struct to pass multiple params (BP)
    if (j) {
        printf("Loader thread could not be created.\n");
    }
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
        sendto(client_socket, packet.data, sizeof(packet.data), 0, NULL, 0);
        // add packet to ack queue
        ack_queue.push(packet);
    }
}

//TODO implement (TM)
int getSequenceNum(Packet * packet) {
    return 1;
}

void formatPacket(Packet * packet) {
    char * tmp = new char [LINE_SIZE];
    unsigned char * checksum = new unsigned char [BLOCK_SIZE];

    strcat(tmp, ":");
    strcat(tmp, to_string(packet->packet_num).c_str());
    strcat(tmp, ";");
    strcat(tmp, packet->payload);
    createChecksum(tmp, checksum);

    strcat(packet->data, (char*) checksum);
    strcat(packet->data, tmp);
}

void *loaderThread(void *arg, void *num_threads, void* filepath, void* line_size) {
    FILE *file = fopen((char*)filepath, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int current_thread = 0;
    char line[*(int*)line_size];

    while (fgets(line, sizeof(line), file) != NULL) {
        Packet packet;
        strncpy(packet.payload, line, sizeof(packet.payload));
        formatPacket(&packet);

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

        current_thread = (current_thread + 1) % *(int*)num_threads;
    }

    fclose(file);
    return NULL;
}

void ackRequestHandler(string buffer){
    // TODO parse buffer to get packet number (test TM)
    int packet_num = 0;
    string rest_of_message = buffer.substr(4);
    stringstream ss(rest_of_message);
    ss >> packet_num;

    // TODO remove packet from ack queue (TM) NOTE: May not pop the correct packet
    while (ack_queue.front().packet_num != packet_num) {
        ack_queue.pop();
    }
}

void nackRequestHandler(string buffer){
    // TODO parse buffer to get packet number (test TM)
    int packet_num = 0;
    string rest_of_message = buffer.substr(5);
    stringstream ss(rest_of_message);
    ss >> packet_num;

    // TODO add packet to (TM)
    while (ack_queue.front().packet_num != packet_num) {
        // TODO add packet to udp queue (TM)

    }
}

void *receieveHandler(void *socket_desc, void* filepath, void* udp_queues) {
    printf("RECIEVED MESSAGE FROM CLIENT!\n");  
    int num_threads = NUM_THREADS;
    int line_size = LINE_SIZE;
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
    if (message_type.substr(0, 4) == "init") { // Client sent "init 5001,5002,5003,5004"
        ThreadData *udp_queues = new ThreadData[num_threads];
        initRequestHandler(buffer, udp_queues, num_threads);
    } else if (message_type.substr(0, 3) == "ack") { // Client sent "ack 24"
        ackRequestHandler(buffer);
    } else  if (message_type.substr(0, 4) == "nack") { // Client sent "nack 24"
        nackRequestHandler(buffer);
    }        
    
    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[])
{
    printf("1!\n");   
    string filepath;
    char * buffer;
    

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

    ThreadData udp_queues[num_udp_threads];
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
    int MAX_CONNECTIONS = 1;
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", listen_port);
    pthread_t thread;
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Create a thread to handle the client
        if (pthread_create(&thread, NULL, receieveHandler, (void *)&new_socket, filepath, udp_queues) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }
    return 0;
}