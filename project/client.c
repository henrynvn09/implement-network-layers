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
  char buffer[1024];

  // Initialize sending_buffer and receiving_buffer
  Send_buffer send_buffer[WINDOW_SIZE];
  uint32_t send_l = 0, send_r = 0;

  Receive_buffer receive_buffer[WINDOW_SIZE];
  uint32_t receive_l = 0, receive_r = 0;

  uint32_t last_ack = 0;
  uint8_t last_ack_count = 0;
  bool handshaked = false;
  int server_seq = 0;

  // send a random SEQ A with SYN flag to the server
  int client_seq = get_random_seq();
  packet handshake = new_syn_packet(client_seq);
  sendto(sockfd, &handshake, sizeof(handshake), 0, (struct sockaddr *)&server_addr,
         sizeof(struct sockaddr_in));

  // Timer to resend the lowest seq if no new ACK in 1 second
  struct timeval last_sent_time;
  gettimeofday(&last_sent_time, NULL);

  // Listen loop
  while (1)
  {
    if (!handshaked)
    {
      // Listen for response from server
      packet response;
      int bytes_recvd = receive_packet(sockfd, &response, &server_addr, &s);

      // if the response is A+1, and SEQ B, send ACK B+1 to the server
      if (bytes_recvd > 0 && is_syn_packet(response) && is_ack_packet(response) && response.ack == client_seq + 1)
      {
        client_seq += 1;
        server_seq = response.seq + 1;

        packet ack = new_ack_packet(client_seq, server_seq);
        send_packet(sockfd, &ack, &server_addr);

        handshaked = true;
        printf("Handshake complete\n");
      }
    }
    else
      continue; // Handshake is done, continue to the next step

    //* ========== Now the connection is established ==========

    //* ===== send data from stdin if there is window is not full
    if (!is_full(&send_l, &send_r))
    {
      int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
      if (bytes_read > 0)
      {
        // save the data to the buffer
        increment_window(&send_r);
        send_buffer[send_r].seq = client_seq;
        client_seq += bytes_read;
        send_buffer[send_r].length = bytes_read;

        // send the data to the server TODO: there may need to mitigate ACK to here
        packet *data_packet = new_data_packet(0, send_buffer[send_r].seq, send_buffer[send_r].length, 0, buffer);
        send_packet(sockfd, data_packet, &server_addr);

        send_buffer[send_r].pac = data_packet;
      }
    }

    //* ==== receive data from the server
    packet server_packet;
    int bytes_recvd = receive_packet(sockfd, &server_packet, &server_addr, &s);
    if (bytes_recvd > 0)
    {
      // if the response is ACK, mark the data as received
      if (is_ack_packet(server_packet))
      {
        remove_acked_sent_buffer(server_packet.ack, &send_buffer, &send_l, &send_r);

        if (last_ack == server_packet.ack)
        {
          last_ack_count++;
          if (last_ack_count == 3)
          {
            // resend the lowest seq in the buffer
            send_packet(sockfd, &send_buffer[send_l], &server_addr);
            last_ack_count = 0; // TODO: not sure 1 or 0
          }
        }
        else
        {
          last_ack = server_packet.ack;
          last_ack_count = 1;
        }
      }

      // handle receiving data

      // place the data in the receive buffer,
      // if the receive seq is in the expected window print it to the stdout
      add_packet_to_data_buffer(&server_packet, &receive_buffer, &receive_l, &receive_r);
      print_data_buffer(&receive_buffer, &receive_l, &receive_r, &server_seq);

      // send ACK with data to the server if send_buffer is not full
      if (!is_full(&send_l, &send_r))
      {
        int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
        if (bytes_read > 0)
        {
          // save the data to the buffer
          increment_window(&send_r);
          send_buffer[send_r].seq = client_seq;
          client_seq += bytes_read;
          send_buffer[send_r].length = bytes_read;

          // send the data to the server TODO: there may need to mitigate ACK to here
          packet *data_packet = new_data_packet(server_seq, send_buffer[send_r].seq, send_buffer[send_r].length, ACK_FLAG, buffer);
          send_buffer[send_r].pac = data_packet;

          send_packet(sockfd, &send_buffer[send_r].pac, &server_addr);
        }
      }
      else
      { // Otherwise, send an ACK with no data
        packet ack = new_ack_packet(client_seq, server_seq);
        send_packet(sockfd, &ack, &server_addr);
      }
    }

    //* ==== retransmit the lowest seq in the buffer if no new ACK in 1 second
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    if (current_time.tv_usec - last_sent_time.tv_usec >= 1e6)
    {
      send_packet(sockfd, &send_buffer[send_l].pac, &server_addr);
      gettimeofday(&last_sent_time, NULL);
    }
  }

  return 0;
}