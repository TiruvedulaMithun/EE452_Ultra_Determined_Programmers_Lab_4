#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>

#include "verification.h"


int main(int argc, char *argv[])
{
    int sock, length, n;
    socklen_t fromlen;
    struct sockaddr_in server;
    struct sockaddr_in from;
    char buf[1024], response[100];

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(0);
    }

    sock=socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("Unable to open socket");
        exit(0);
    }
    length = sizeof(server);
    bzero(&server,length);
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=INADDR_ANY;
    server.sin_port=htons(atoi(argv[1]));
    if (bind(sock,(struct sockaddr *)&server,length)<0) {
        printf("Unable to bind socket");
        exit(0);
    }
    fromlen = sizeof(struct sockaddr_in);
    while (1) {
        n = recvfrom(sock,buf,1024,0,(struct sockaddr *)&from,&fromlen);
        if (n < 0) {
            printf("Unable to use recvfrom");
            exit(0);
        }
        
        // if(verifyChecksum(buf)) {
        //     response = ":ACK";
        // }
        // else {
        //     response = ":NACK";
        // }
        printf("Received: %s\n", buf);

        n = sendto(sock, response, 17, 0, (struct sockaddr *)&from, fromlen);
        if (n < 0) {
            printf("Unable to use sendto");
            exit(0);
        }
    }
    return 0;
}