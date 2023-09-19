#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <fstream>
#include <sstream>

#include "verification.h"
#include "packet.h"

void readPacket(char data[], Packet *packet) {
    *packet = Packet();
    bool pay_found;
    string str_data = data;
    int begin = -1;
    int end = -1;

    for(int i = 0; i < strlen(data); i++) {
        if(data[i] == ';') {
            pay_found = true;
            end = i+1;
        }
        if(pay_found) {
            packet->payload[i-end] = data[i];
        }
    }

    begin = str_data.find_first_of(':');
    try
    {
        packet->packet_num = stoi(str_data.substr(begin+1, end-(begin+1)-1));
    }
    catch(const std::exception& e)
    {
        packet->packet_num = -1;
    }
}

void writeFile(std::ofstream & file, char payload []) {
    if (file.is_open()) {
        file.write(payload, strlen(payload));
    }
    else {
        printf("File not open");
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    int udp_sock, tcp_sock, udp_port, tcp_port, n, next_packet, num_packets;
    struct sockaddr_in udp_server, tcp_server, from;
    char buf[PACKET_SIZE], response[100];
    struct hostent *server_ip;
    socklen_t fromlen, udp_length, tcp_length;
    string filepath;
    Packet packet;
    ofstream file;
    char ** payloads;
    bool * acked;

    bool done = false;
    bool init = false;

    if (argc != 5) {
        fprintf(stderr, "ERROR, missing information. Please use the following format.\n \
./custom_ftp_receiver <server_hostname> <server_udp_port> <server_tcp_port> <local_filepath_to_store>\n");
        exit(0);
    }

    server_ip = gethostbyname(argv[1]);
    if (server_ip==0) {
        printf("Unknown host");
        exit(0);
    }
    udp_port = atoi(argv[2]);
    tcp_port = atoi(argv[3]);
    filepath = argv[4];

    tcp_sock=socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        printf("Unable to open socket");
        exit(0);
    }

    tcp_length = sizeof(tcp_server);
    bzero(&tcp_server,tcp_length);
    tcp_server.sin_family=AF_INET;
    memcpy(&tcp_server.sin_addr, server_ip->h_addr_list[0], server_ip->h_length);
    tcp_server.sin_port=htons(tcp_port);
    n = connect(tcp_sock,(struct sockaddr *) &tcp_server, tcp_length);
    if (n < 0) {
        printf("ERROR connecting %d", n);
    }

    //Create UDP sockets
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        printf("Unable to create UDP socket for sending file");
        exit(0);
    }
    udp_length = sizeof(udp_server);
    bzero(&udp_server,udp_length);
    udp_server.sin_family=AF_INET;
    udp_server.sin_addr.s_addr = INADDR_ANY;
    udp_server.sin_port=htons(udp_port);
    if (bind(udp_sock,(struct sockaddr *)&udp_server,udp_length)<0) {
        printf("Unable to bind socket");
        exit(0);
    }

    strcpy(response, "Start");
    n = write(tcp_sock, response, sizeof(response));//, 0, (struct sockaddr *)&tcp_server, fromlen);
    if (n < 0) {
        printf("Unable to use send");
        exit(0);
    }

    fromlen = sizeof(struct sockaddr_in);

    while (!done) {
        memset(buf, '\0', sizeof(buf));
        n = recvfrom(udp_sock,buf,sizeof(buf),0,(struct sockaddr *)&from, &fromlen);
        if (n < 0) {
            printf("Unable to use recvfrom");
            exit(0);
        }

        // printf("New message: %s\n", buf);
        readPacket(buf, &packet);
        if(packet.packet_num < 0) {
            continue;
        }
        if((init) && (acked[packet.packet_num] == true)){
            continue;
        }
        // printf("Pack:%d\n", packet.packet_num);

        if(verifyChecksum(buf)) {
            strcpy(response, "ACK ");
            strcat(response, to_string(packet.packet_num).c_str());

            // printf("Send: %s\n\n", response);
            n = write(tcp_sock, response, 17);//, 0, (struct sockaddr *)&tcp_server, fromlen);
            if (n < 0) {
                printf("Unable to use send");
                exit(0);
            }

            if ((!init) & (packet.packet_num == 0)) {
                num_packets = stoi(packet.payload);
                payloads = new char*[num_packets];
                acked = new bool[num_packets+1];
                for(int j = 0; j < num_packets; j++) {
                    payloads[j] = new char[LINE_SIZE];
                }
                for(int j = 0; j <= num_packets; j++) {
                    acked[j] = false;
                }
                acked[0] = true;
                init = true;
                next_packet = 1;
            }
            else if (init & (packet.packet_num > 0)) {
                // printf("Pay#%d:%s\n", packet.packet_num, packet.payload);
                acked[packet.packet_num] = true;
                
                // printf("new: %d, expecting:%d", packet.packet_num, next_packet);
                strcpy(payloads[packet.packet_num - 1], packet.payload);
            }
        }
        done = init;
//        printf("num:%d", num_packets);
        for(int x = 0; x < num_packets; x++) {
            done &= acked[x];
        }
        
        memset(response, '\0', sizeof(response));
    }
    file.open(filepath);
    if (file.is_open()) {
        for (int i = 0; i < num_packets; i++) {
            file.write(payloads[i], strlen(payloads[i]));
        }
        file.close();
    }
    else {
        printf("Could not open %s", filepath.c_str());
        exit(0);
    }
    printf("Wrote to %s", filepath.c_str());

    close(udp_sock);
    close(tcp_sock);
    return 0;
}
