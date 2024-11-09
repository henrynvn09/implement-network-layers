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
#include "ultilities.h"

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

  // Listen loop
  while (1)
  {
    // Receive from socket
    int bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    client_connected =
        1; // At this point, the client has connected and sent data

    // Data available to write
    if (bytes_recvd > 0)
    {
      write(STDOUT_FILENO, buffer, bytes_recvd);
    }

    // Read from stdin
    int bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));

    // Data available to send from stdin
    if (bytes_read > 0)
    {
      sendto(sockfd, &buffer, bytes_read, 0, (struct sockaddr *)&client_addr,
             sizeof(struct sockaddr_in));
    }
  }

  return 0;
}