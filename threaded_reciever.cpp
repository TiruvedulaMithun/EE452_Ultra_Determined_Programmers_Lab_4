#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>

#define PACKET_WINDOW 10 // Number of packets to collect before writing to file

// Structure to pass data to UDP listening threads
typedef struct {
    int udp_socket;
    pthread_mutex_t mutex;
    int queue_size;
} ThreadData;

char *FILE_QUEUE[PACKET_WINDOW];

void *udpListenThread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char buffer[1024];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    while (1) {
        int recv_len = recvfrom(data->udp_socket, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&server_addr, &addr_len);
        if (recv_len > 0) {
            // Handle received data as needed
            buffer[recv_len] = '\0';

            // TODO verify checksum
            
            // TODO send ACK or NACK

            // Add received data to the queue with mutex protection
            pthread_mutex_lock(&data->mutex);
            if (data->queue_size < PACKET_WINDOW) {
                FILE_QUEUE[data->queue_size++] = strdup(buffer);
            }
            pthread_mutex_unlock(&data->mutex);
        }
    }

    return NULL;
}

void *fileWriterThread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    FILE *file = fopen("received_data.txt", "w");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int packets_written = 0;

    while (1) {
        if (packets_written >= PACKET_WINDOW) {
            // TODO check packet number and order packets in queue before writing to file


            pthread_mutex_lock(&data->mutex);
            for (int i = 0; i < PACKET_WINDOW; i++) {
                fprintf(file, "%s\n", FILE_QUEUE[i]);
                free(FILE_QUEUE[i]);
                FILE_QUEUE[i] = NULL;
            }
            data->queue_size = 0;
            pthread_mutex_unlock(&data->mutex);
            packets_written = 0;
        }
    }

    fclose(file);
    return NULL;
}

int main(int argc, char *argv[]) {
    printf("1!\n");   

    if (argc != 3) {
        fprintf(stderr, "ERROR, missing information. Please use the following format.\n \
./custom_ftp <listen_port> <number_of_UDP_threads> <local_filepath_to_transfer>\n");
        exit(0);
    }
    
    //Parse command line arguments
    char* SERVER_IP = argv[1];
    int SERVER_PORT = atoi(argv[2]);
    int NUM_UDP_PORTS = atoi(argv[2]);
    char* filepath = argv[4];

    int tcp_socket;
    pthread_t udp_threads[NUM_UDP_PORTS];
    ThreadData udp_thread_data[NUM_UDP_PORTS];
    struct sockaddr_in server_addr;
    char udp_ports[1024]; // Buffer to store comma-separated UDP ports

    // Create TCP socket
    if ((tcp_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP connection failed");
        exit(EXIT_FAILURE);
    }

    // Generate and send comma-separated UDP port numbers to the server
    char udp_ports_string[1024];
    for (int i = 0; i < NUM_UDP_PORTS; i++) {
        udp_thread_data[i].udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_thread_data[i].udp_socket < 0) {
            perror("UDP socket creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in udp_addr;
        udp_addr.sin_family = AF_INET;
        udp_addr.sin_port = 0; // Let the OS assign an available port
        udp_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(udp_thread_data[i].udp_socket, (struct sockaddr *)&udp_addr, sizeof(udp_addr)) < 0) {
            perror("UDP socket binding failed");
            exit(EXIT_FAILURE);
        }

        // Create a thread to listen on this UDP port
        if (pthread_create(&udp_threads[i], NULL, udpListenThread, &udp_thread_data[i]) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in bound_addr;
        socklen_t addr_len = sizeof(bound_addr);
        getsockname(udp_thread_data[i].udp_socket, (struct sockaddr *)&bound_addr, &addr_len);

        sprintf(udp_ports_string, "%s%d,", udp_ports_string, ntohs(bound_addr.sin_port));
    }

    // Remove the trailing comma
    udp_ports_string[strlen(udp_ports_string) - 1] = '\0';

    // Create a thread to write the queue to a file
    pthread_t file_writer_thread;
    ThreadData file_writer_data;
    file_writer_data.queue_size = 0;
    pthread_mutex_init(&file_writer_data.mutex, NULL);

    if (pthread_create(&file_writer_thread, NULL, fileWriterThread, &file_writer_data) != 0) {
        perror("File writer thread creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Send the UDP port numbers to the server over TCP
    send(tcp_socket, udp_ports_string, strlen(udp_ports_string), 0);
    printf("Sent UDP ports to the server: %s\n", udp_ports_string);




    // // WHEN FINISHED
    // // Close the TCP socket (you can keep the UDP sockets open and listening)
    // close(tcp_socket);

    // // Join UDP listening threads (optional, you may choose when to terminate them)
    // for (int i = 0; i < NUM_UDP_PORTS; i++) {
    //     pthread_join(udp_threads[i], NULL);
    // }

    // // Join the file writer thread (optional, you may choose when to terminate it)
    // pthread_join(file_writer_thread, NULL);

    return 0;
}
