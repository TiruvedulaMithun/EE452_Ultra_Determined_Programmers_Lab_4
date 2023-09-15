
const static int PACKET_SIZE = 1500;
const static int LINE_SIZE = 1480;

typedef struct {
    char payload[LINE_SIZE];
    char data[PACKET_SIZE];
    int packet_num;
} Packet;