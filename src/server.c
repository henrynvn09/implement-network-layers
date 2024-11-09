#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <string.h>
#include "ultilities.c"

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: server <port>\n");
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

  /* Construct our address */
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;         // use IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY; // accept all connections
                                            // same as inet_addr("0.0.0.0")
                                            // "Address string to network bytes"
  // Set receiving port
  int PORT = atoi(argv[1]);
  server_addr.sin_port = htons(PORT); // Big endian

  /* Let operating system know about our config */
  int did_bind =
      bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  // Error if did_bind < 0 :(
  if (did_bind < 0)
    return errno;

  struct sockaddr_in client_addr; // Same information, but about client
  socklen_t s = sizeof(struct sockaddr_in);
  char buffer[1024];

  int client_connected = 0;

  // Initialize sending_buffer and receiving_buffer
  packet *send_buffer[WINDOW_SIZE];
  uint32_t send_l = 0, send_r = 0;

  Receive_buffer receive_buffer[WINDOW_SIZE];
  uint32_t receive_l = 0, receive_r = 0;

  uint32_t last_ack = 0;
  uint8_t last_ack_count = 0;
  bool first_SYN = false;
  bool handshaked = false;

  // Initialize the server_seq
  uint32_t server_seq = get_random_seq();
  uint32_t client_seq = 0;

  // Timer to resend the lowest seq if no new ACK in 1 second
  struct timeval last_sent_time;
  gettimeofday(&last_sent_time, NULL);

  // Listen loop
  while (1)
  {
    packet p;
    int bytes_recvd = recvfrom(sockfd, &p, sizeof(p), 0,
                               (struct sockaddr *)&client_addr, &s);
    if (!handshaked)
    {
      if (!first_SYN && bytes_recvd > 0)
      {
        if (is_syn_packet(p))
        {
          // Send SYN+ACK to client
          packet *syn_ack_packet = new_data_packet(p.seq + 1, server_seq, 0, SYN_FLAG | ACK_FLAG, NULL);
          server_seq += 1;
          client_seq = p.seq + 1;
          send_packet(sockfd, syn_ack_packet, &client_addr);
        }

        first_SYN = true;
      }
      else if (first_SYN && bytes_recvd > 0)
      {
        // Listen for ACK with SEQ from client
        if (is_ack_packet(p) && p.ack == server_seq)
        {
          handshaked = true;
          printf("Handshake complete\n");
        }
      }
      continue;
    }

    //* ========== Now the connection is established ==========

    //* ===== send data from stdin if there is window is not full
    if (!is_full(&send_l, &send_r))
    {
      int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
      if (bytes_read > 0)
      {
        // send the data to the client TODO: there may need to mitigate ACK to here
        packet *data_packet = new_data_packet(0, server_seq, bytes_read, 0, buffer);
        send_packet(sockfd, data_packet, &client_addr);

        // save the data to the buffer
        increment_window(&send_r);
        send_buffer[send_r] = data_packet;
        server_seq += bytes_read;
      }
    }

    //* ==== receive data from the client
    if (bytes_recvd > 0)
    {
      // if the response is ACK, mark the data as received
      if (is_ack_packet(p))
      {
        remove_acked_sent_buffer(p.ack, send_buffer, &send_l, &send_r);

        if (last_ack == p.ack)
        {
          last_ack_count++;
          if (last_ack_count == 3)
          {
            // resend the lowest seq in the buffer
            send_packet(sockfd, send_buffer[send_l], &client_addr);
            last_ack_count = 0; // TODO: not sure 1 or 0
          }
        }
        else
        {
          last_ack = p.ack;
          last_ack_count = 1;
        }
      }

      // handle receiving data

      // place the data in the receive buffer,
      // if the receive seq is in the expected window print it to the stdout
      add_packet_to_data_buffer(&p, &receive_buffer, &receive_l, &receive_r);
      print_data_buffer(&receive_buffer, &receive_l, &receive_r, &server_seq);

      // send ACK with data to the client if send_buffer is not full
      if (!is_full(&send_l, &send_r))
      {
        int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
        if (bytes_read > 0)
        {
          // send the data to the server TODO: there may need to mitigate ACK to here
          packet *data_packet = new_data_packet(client_seq, server_seq, bytes_read, ACK_FLAG, buffer);

          send_packet(sockfd, data_packet, &client_addr);

          // save the data to the buffer
          increment_window(&send_r);
          send_buffer[send_r] = data_packet;
          server_seq += bytes_read;
        }
      }
      else
      { // Otherwise, send an ACK with no data
        packet *ack = new_ack_packet(server_seq, client_seq);
        send_packet(sockfd, ack, &client_addr);
      }
    }
  }

  return 0;
}