#include "ultilities.h"

//* ================== DEBUGGING ==================
#define RECV 0
#define SEND 1
#define RTOS 2
#define DUPA 3

static inline void print_diag(packet *pkt, int diag)
{
	switch (diag)
	{
	case RECV:
		fprintf(stderr, "- RECV");
		break;
	case SEND:
		fprintf(stderr, "- SEND");
		// fprintf(stderr, " - SEND");
		break;
	case RTOS:
		fprintf(stderr, "- RTOS");
		break;
	case DUPA:
		fprintf(stderr, "- DUPS");
		break;
	}

	bool syn = pkt->flags & 0b01;
	bool ack = pkt->flags & 0b10;
	fprintf(stderr, " %u ACK %u SIZE %hu FLAGS ", ntohl(pkt->seq),
			ntohl(pkt->ack), ntohs(pkt->length));
	if (!syn && !ack)
	{
		fprintf(stderr, "NONE");
	}
	else
	{
		if (syn)
		{
			fprintf(stderr, "SYN ");
		}
		if (ack)
		{
			fprintf(stderr, "ACK ");
		}
	}
	fprintf(stderr, "\n");
}

//* ================== Helper functions ==================

uint32_t get_random_seq()
{
	int r;
	srand(time(NULL));
	r = rand() % (INT64_MAX / 2);
	return r;
}

packet *new_syn_packet(uint32_t seq)
{
	packet *p = malloc(sizeof(packet));
	p->ack = 0;
	p->seq = seq;
	p->length = 0;
	p->flags = SYN_FLAG;
	p->unused = 0;
	return p;
}

packet *new_ack_packet(uint32_t seq, uint32_t ack)
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
	memset(p->payload, 0, sizeof(p->payload));
	memcpy(p->payload, payload, length);

	return p;
}

int receive_packet(int sockfd, packet *p, struct sockaddr_in *server_addr, socklen_t *s)
{
	int bytes_recvd = recvfrom(sockfd, p, sizeof(*p), 0,
							   (struct sockaddr *)server_addr, s);

	if (bytes_recvd > 0)
	{
		print_diag(p, RECV);
	}

	convert_packet_receiving_endian(p);
	return bytes_recvd;
}

void send_packet(int sockfd, packet *p, struct sockaddr_in *target_addr)
{
	if (p == NULL)
		return;
	// simulate packet loss
	// if (p->length > 0 && rand() % 100 < 10)
	// {
	// 	fprintf(stderr, "========== Dropping packet =========\n");
	// 	return;
	// }
	convert_packet_sending_endian(p);

	print_diag(p, SEND);
	sendto(sockfd, p, sizeof(*p), 0, (struct sockaddr *)target_addr, sizeof(struct sockaddr_in));
	convert_packet_receiving_endian(p);
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

bool is_syn_packet(packet *p)
{
	return (p->flags & 1) != 0;
}

bool is_ack_packet(packet *p)
{
	return (p->flags & 0b10) != 0;
}

void increment_window(int *ptr)
{
	*ptr = (*ptr + 1) % WINDOW_SIZE;
}

void add_packet_to_receive_buffer(packet *p, Receive_buffer **buffer, uint32_t expected_seq)
{
	if (p->seq < expected_seq)
	{
		fprintf(stderr, "\t\tPacket already received\n");
		return;
	}
	Receive_buffer *new_buffer = malloc(sizeof(Receive_buffer));
	new_buffer->seq = p->seq;
	new_buffer->length = p->length;
	new_buffer->next = NULL;
	memset(new_buffer->data, 0, sizeof(new_buffer->data));
	memcpy(new_buffer->data, p->payload, p->length);

	Receive_buffer *head = *buffer, *prev = NULL;

	while (head != NULL)
	{
		if (head->seq == p->seq)
		{
			fprintf(stderr, "\t\tPacket already in buffer\n");
			return;
		}

		if (head->seq > p->seq)
		{
			new_buffer->next = head;
			if (prev)
				prev->next = new_buffer;
			else // if the new packet is the smallest
				*buffer = new_buffer;
			return;
		}
		prev = head;
		head = head->next;
	}

	// append the packet to the end of the buffer
	if (prev)
		prev->next = new_buffer;
	else
		*buffer = new_buffer;
}

void remove_acked_sent_buffer(uint32_t server_ack, packet *send_buffer[WINDOW_SIZE], int *send_l, int *send_r)
{
	// iterate through sent buffer to remove SEQ less than ACK
	while (!is_empty(send_l, send_r) && send_buffer[*send_l]->seq < server_ack)
	{
		free(send_buffer[*send_l]);
		send_buffer[*send_l] = NULL;
		increment_window(send_l);
	}
}

void output_data_buffer(Receive_buffer **buffer, uint32_t *expected_seq)
{
	while ((*buffer) && (*buffer)->seq == *expected_seq)
	{
		// fprintf(stderr, "popping %u\n", (*buffer)->seq);
		write(STDOUT_FILENO, (*buffer)->data, (*buffer)->length);
		(*expected_seq) += (*buffer)->length;
		Receive_buffer *temp = (*buffer);
		(*buffer) = (*buffer)->next;
		free(temp);
		temp = NULL;
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

void debug_receive_buffer(Receive_buffer **buffer)
{
	fprintf(stderr, "\t++Buffer: ");
	Receive_buffer *head = *buffer;
	while (head != NULL)
	{
		fprintf(stderr, "%u ", head->seq);
		head = head->next;
	}
	fprintf(stderr, "\n");
}
