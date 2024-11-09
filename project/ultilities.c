
#define MSS 1012		 // MSS = Maximum Segment Size (aka max length)
#define MEMCPY_SIZE 1024 // size of the data to be copied
#define MAX_SEQ 1000000	 // TODO: Maximum sequence number
#define SYN_FLAG 0b1
#define ACK_FLAG 0b10
#define WINDOW_SIZE 21
// #include "ultilities.h"

typedef struct
{
	uint32_t ack;	 // next byte that the receiver is expecting
	uint32_t seq;	 // represents the first byte number in this packet
	uint16_t length; // length of the payload in bytes
	uint8_t flags;	 // flag 0b(ACK)(SYN)
	uint8_t unused;
	uint8_t payload[MSS];
} packet;

typedef struct
{
	char data[1024];
	uint32_t seq;
	uint16_t length;
} Data_buffer;

int get_random_seq()
{
	return rand() % (INT16_MAX / 2);
}

packet new_syn_packet(int seq)
{
	packet p = {0, seq, 0, SYN_FLAG, 0, {0}};
	return p;
}

packet new_ack_packet(int seq, int ack)
{
	packet p = {ack, seq, 0, ACK_FLAG, 0, {0}};
	return p;
}

packet new_data_packet(uint32_t ack, uint32_t seq, uint16_t length, uint8_t flags, uint8_t *payload)
{
	packet p = {ack, seq, length, flags, 0, {0}};
	memcpy(p.payload, payload, length);
	return p;
}

int receive_packet(int sockfd, packet *p, struct sockaddr_in *server_addr, socklen_t *s)
{
	int bytes_recvd = recvfrom(sockfd, p, sizeof(*p), 0,
							   (struct sockaddr *)server_addr, s);
	convert_packet_receiving_endian(p);
	return bytes_recvd;
}

int send_packet(int sockfd, packet *p, struct sockaddr_in *server_addr)
{
	convert_packet_sending_endian(p);
	sendto(sockfd, p, sizeof(*p), 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in));
}

void convert_packet_receiving_endian(packet *p)
{
	p->ack = ntohl(p->ack);
	p->seq = ntohl(p->seq);
	p->length = ntohs(p->length);
}

void convert_packet_sending_endian(packet *p)
{
	p->ack = htonl(p->ack);
	p->seq = htonl(p->seq);
	p->length = htons(p->length);
}

bool is_syn_packet(packet p)
{
	return (p.flags & 1) != 0;
}

bool is_ack_packet(packet p)
{
	return (p.flags & 0b10) != 0;
}

void increment_window(int *ptr)
{
	*ptr = (*ptr + 1) % WINDOW_SIZE;
}

void add_packet_to_data_buffer(packet *p, Data_buffer *buffer, int *l, int *r)
{
	if (is_full(l, r))
	{
		printf("Buffer is full\n");
		return;
	}

	for (int i = *l; i != *r; i = (i + 1) % WINDOW_SIZE)
	{
		if (buffer[i].seq == p->seq)
			return;

		if (buffer[i].seq > p->seq)
		{
			// insert the packet to the buffer
			for (int j = *r; j != i; j = (j - 1 + WINDOW_SIZE) % WINDOW_SIZE)
			{
				memcpy(&buffer[j], &buffer[(j - 1 + WINDOW_SIZE) % WINDOW_SIZE], sizeof(Data_buffer));
			}
			increment_window(r);

			buffer[i].seq = p->seq;
			buffer[i].length = p->length;
			memcpy(buffer[i].data, p->payload, p->length);

			return;
		}
	}

	// append the packet to the end of the buffer
	increment_window(r);
	buffer[*r].seq = p->seq;
	buffer[*r].length = p->length;
	memcpy(buffer[*r].data, p->payload, p->length);
}

void remove_acked_sent_buffer(uint32_t server_ack, Data_buffer *send_buffer, int *send_l, int *send_r)
{
	// iterate through sent buffer to remove SEQ less than ACK
	while (!is_empty(send_l, send_r) && send_buffer[*send_l].seq < server_ack)
	{
		increment_window(send_l);
	}
}

void print_data_buffer(Data_buffer *buffer, int *l, int *r, int *expected_seq)
{
	if (is_full(l, r))
	{
		return;
	}

	while (!is_empty(l, r) && buffer[*l].seq == *expected_seq)
	{
		write(STDOUT_FILENO, buffer[*l].data, buffer[*l].length);
		increment_window(l);
		*expected_seq += buffer[*l].length;
	}
}

bool is_full(int *l, int *r)
{
	return *l == (*r + 1) % WINDOW_SIZE;
}
bool is_empty(int *l, int *r)
{
	return *l == *r;
}
