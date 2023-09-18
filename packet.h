
const static int PACKET_SIZE = 1500;
const static int LINE_SIZE = 1400;
const static int BUFFER_SIZE = 100;

typedef struct {
    char payload[LINE_SIZE];
    char data[PACKET_SIZE];
    int packet_num;
} Packet;