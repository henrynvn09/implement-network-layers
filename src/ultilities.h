
typedef struct {
	uint32_t ack; // next byte that the receiver is expecting
	uint32_t seq; // represents the first byte number in this packet
	uint16_t length; // length of the payload in bytes
	uint8_t flags; // flag 0b(ACK)(SYN)
	uint8_t unused;
	uint8_t payload[MSS];
} packet;


int get_random_seq();
packet new_syn_packet(int seq);
packet new_ack_packet(int seq, int ack);
packet new_data_packet(int seq, int ack, char *data, int length);
