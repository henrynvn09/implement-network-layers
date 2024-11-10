#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MSS 1012 // MSS = Maximum Segment Size (aka max length)
#define SYN_FLAG 0b1
#define ACK_FLAG 0b10
#define WINDOW_SIZE 21

typedef struct
{
    uint32_t ack;    // next byte that the receiver is expecting
    uint32_t seq;    // represents the first byte number in this packet
    uint16_t length; // length of the payload in bytes
    uint8_t flags;   // flag 0b(ACK)(SYN)
    uint8_t unused;
    uint8_t payload[MSS];
} packet;

typedef struct Receive_buffer Receive_buffer;

struct Receive_buffer
{
    uint32_t seq;
    uint16_t length;
    uint8_t data[MSS];
    struct Receive_buffer *next;
};

uint32_t get_random_seq();
packet *new_syn_packet(uint32_t seq);
packet *new_ack_packet(uint32_t seq, uint32_t ack);
packet *new_data_packet(uint32_t ack, uint32_t seq, uint16_t length, uint8_t flags, uint8_t *payload);
int receive_packet(int sockfd, packet *p, struct sockaddr_in *server_addr, socklen_t *s);
void send_packet(int sockfd, packet *p, struct sockaddr_in *target_addr);
void convert_packet_receiving_endian(packet *p);
void convert_packet_sending_endian(packet *p);
bool is_syn_packet(packet *p);
bool is_ack_packet(packet *p);
void increment_window(int *ptr);
void add_packet_to_receive_buffer(packet *p, Receive_buffer **buffer);
void remove_acked_sent_buffer(uint32_t server_ack, packet *send_buffer[WINDOW_SIZE], int *send_l, int *send_r);
void output_data_buffer(Receive_buffer **buffer, uint32_t *expected_seq);
bool is_full(int *l, int *r);
bool is_empty(int *l, int *r);
void debug_receive_buffer(Receive_buffer **buffer);

#endif // UTILITIES_H