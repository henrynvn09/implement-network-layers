#include "ultilities.h"

int get_random_seq()
{
	return rand() % (INT16_MAX / 2);
}

packet *new_syn_packet(int seq)
{
	packet *p = malloc(sizeof(packet));
	p->ack = 0;
	p->seq = seq;
	p->length = 0;
	p->flags = SYN_FLAG;
	p->unused = 0;
	return p;
}

packet *new_ack_packet(int seq, int ack)
{
	packet *p = malloc(sizeof(packet));
	p->ack = ack;
	p->seq = seq;
	p->length = 0;
	p->flags = ACK_FLAG;
	p->unused = 0;
	return p;
}

packet *new_data_packet(uint32_t ack, uint32_t seq, uint16_t length, uint8_t flags, uint8_t *payload)
{
	packet *p = malloc(sizeof(packet));
	p->ack = ack;
	p->seq = seq;
	p->length = length;
	p->flags = flags;
	p->unused = 0;
	memset(p->payload, 0, MSS);
	memcpy(p->payload, payload, length);

	return p;
}

int receive_packet(int sockfd, packet *p, struct sockaddr_in *server_addr, socklen_t *s)
{
	int bytes_recvd = recvfrom(sockfd, p, sizeof(*p), 0,
							   (struct sockaddr *)server_addr, s);
	convert_packet_receiving_endian(p);
	return bytes_recvd;
}

void send_packet(int sockfd, packet *p, struct sockaddr_in *target_addr)
{
	convert_packet_sending_endian(p);
	sendto(sockfd, p, sizeof(*p), 0, (struct sockaddr *)target_addr, sizeof(struct sockaddr_in));
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

void add_packet_to_data_buffer(packet *p, Receive_buffer *buffer, int *l, int *r)
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
				memcpy(&buffer[j], &buffer[(j - 1 + WINDOW_SIZE) % WINDOW_SIZE], sizeof(Receive_buffer));
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

void remove_acked_sent_buffer(uint32_t server_ack, packet *send_buffer[WINDOW_SIZE], int *send_l, int *send_r)
{
	// iterate through sent buffer to remove SEQ less than ACK
	while (!is_empty(send_l, send_r) && send_buffer[*send_l]->seq < server_ack)
	{
		free(send_buffer[*send_l]);
		increment_window(send_l);
	}
}

void print_data_buffer(Receive_buffer *buffer, int *l, int *r, uint32_t *expected_seq)
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
