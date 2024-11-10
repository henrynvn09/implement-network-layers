#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include "ultilities.c"

#define WINDOW_SIZE 21

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage: client <hostname> <port> \n");
    exit(1);
  }

  /* Create sockets */
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  // use IPv4  use UDP
  // Error if socket could not be created
  if (sockfd < 0)
    return errno;

  // Set socket for nonblocking
  int flags = fcntl(sockfd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(sockfd, F_SETFL, flags);

  // Setup stdin for nonblocking
  flags = fcntl(STDIN_FILENO, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(STDIN_FILENO, F_SETFL, flags);

  /* Construct server address */
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET; // use IPv4
  // Only supports localhost as a hostname, but that's all we'll test on
  char *addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
  server_addr.sin_addr.s_addr = inet_addr(addr);
  // Set sending port
  int PORT = atoi(argv[2]);
  server_addr.sin_port = htons(PORT); // Big endian
  socklen_t s = sizeof(struct sockaddr_in);
  uint8_t buffer[MSS];

  // Initialize sending_buffer and receiving_buffer
  packet *send_buffer[WINDOW_SIZE];
  int send_l = 0, send_r = 0;

  Receive_buffer *receive_buffer = NULL;

  uint32_t last_ack = 0;
  uint8_t last_ack_count = 0;
  bool handshaked = false;
  uint32_t server_seq = 0;

  // send a random SEQ A with SYN flag to the server
  uint32_t client_seq = get_random_seq();
  fprintf(stderr, "Client SEQ: %u\n", client_seq);
  packet *first_SYN = new_syn_packet(client_seq);
  send_packet(sockfd, first_SYN, &server_addr);
  // free(first_SYN);

  // Timer to resend the lowest seq if no new ACK in 1 second
  struct timeval last_ack_time;
  gettimeofday(&last_ack_time, NULL);
  struct timeval time_diff;

  // Listen loop
  while (1)
  {
    if (!handshaked)
    {
      // Listen for response from server
      packet response;
      int bytes_recvd = receive_packet(sockfd, &response, &server_addr, &s);

      // if the response is A+1, and SEQ B, send ACK B+1 to the server
      if (bytes_recvd > 0 && is_syn_packet(&response) && is_ack_packet(&response) && response.ack == client_seq + 1)
      {
        client_seq += 1;
        server_seq = response.seq + 1;

        packet *ack = new_ack_packet(client_seq, server_seq);
        send_packet(sockfd, ack, &server_addr);

        handshaked = true;
        fprintf(stderr, "Handshake complete\n");
      }
      continue;
    }
    //* ========== Now the connection is established ==========
    //* ==== retransmit the lowest seq in the buffer if no new ACK in 1 second
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    struct timeval time_diff;
    timersub(&current_time, &last_ack_time, &time_diff);

    if (time_diff.tv_sec >= 1 || time_diff.tv_usec >= 1e6)
    {
      if (!is_empty(&send_l, &send_r))
        send_packet(sockfd, send_buffer[send_l], &server_addr);
      gettimeofday(&last_ack_time, NULL);
    }

    //* ==== receive data from the server
    packet server_packet;
    int bytes_recvd = receive_packet(sockfd, &server_packet, &server_addr, &s);
    if (bytes_recvd > 0)
    {
      // fprintf(stderr, "client_seq: %u, server_seq: %u\n", client_seq, server_seq);
      // if the response is ACK, mark the data as received
      if (is_ack_packet(&server_packet))
      {
        remove_acked_sent_buffer(server_packet.ack, send_buffer, &send_l, &send_r);
        gettimeofday(&last_ack_time, NULL);
        // if last 3 ACKs are the same, resend the lowest seq in the buffer
        if (last_ack == server_packet.ack)
        {
          last_ack_count++;
          if (last_ack_count == 3)
          {
            // resend the lowest seq in the buffer
            if (!is_empty(&send_l, &send_r))
              send_packet(sockfd, send_buffer[send_l], &server_addr);
            last_ack_count = 0; // TODO: not sure 1 or 0
          }
        }
        else
        {
          last_ack = server_packet.ack;
          last_ack_count = 1;
        }
      }
      debug_receive_buffer(&receive_buffer);

      if (server_packet.length == 0)
        continue;

      // handle receiving data

      // place the data in the receive buffer,
      // if the receive seq is in the expected window print it to the stdout
      add_packet_to_receive_buffer(&server_packet, &receive_buffer, server_seq);
      output_data_buffer(&receive_buffer, &server_seq);

      // send ACK with data to the server if send_buffer is not full
      if (!is_full(&send_l, &send_r))
      {
        // fprintf(stderr, "send data to the server with ack\n");
        int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
        if (bytes_read > 0)
        {
          // send the data to the server TODO: there may need to mitigate ACK to here
          packet *data_packet = new_data_packet(server_seq, client_seq, bytes_read, ACK_FLAG, buffer);

          send_packet(sockfd, data_packet, &server_addr);

          // save the data to the buffer
          send_buffer[send_r] = data_packet;
          send_buffer[send_r]->ack = 0;
          send_buffer[send_r]->flags = 0;
          client_seq += bytes_read;
          increment_window(&send_r);
        }
        else
        { // Otherwise, send an ACK with no data
          // fprintf(stderr, "send ack to the server with no data\n");
          packet *ack = new_ack_packet(client_seq, server_seq);
          send_packet(sockfd, ack, &server_addr);
        }
      }
      else
      { // Otherwise, send an ACK with no data
        // fprintf(stderr, "send ack to the server with no data\n");
        packet *ack = new_ack_packet(client_seq, server_seq);
        send_packet(sockfd, ack, &server_addr);
      }
      // fprintf(stderr, "client_seq: %u, server_seq: %u\n", client_seq, server_seq);
    }
    //* ===== send data from stdin if there is window is not full
    else if (!is_full(&send_l, &send_r))
    {
      int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
      if (bytes_read > 0)
      {
        // send the data to the server TODO: there may need to mitigate ACK to here
        packet *data_packet = new_data_packet(0, client_seq, bytes_read, 0, buffer);
        send_packet(sockfd, data_packet, &server_addr);

        // save the data to the buffer
        send_buffer[send_r] = data_packet;
        client_seq += bytes_read;
        increment_window(&send_r);
      }
    }
  }

  return 0;
}