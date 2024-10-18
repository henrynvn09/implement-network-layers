#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3)
        return 1;

    char *hostname = argv[1];
    if (strcmp("localhost", hostname) == 0)
        hostname = "127.0.0.1";

    int port = atoi(argv[2]);

    /* 1. Create socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // use IPv4  use UDP

    /* 2. Construct server address */
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET; // use IPv4
    serveraddr.sin_addr.s_addr = inet_addr(hostname);
    // Set sending port
    // int SEND_PORT = 8080;
    serveraddr.sin_port = htons(port); // Big endian

    int BUF_SIZE = 1024;
    /* 3. Send data to server */
    char client_buf[BUF_SIZE];
    // int did_send = sendto(sockfd, client_buf, strlen(client_buf),
    //                    // socket  send data   how much to send
    //                       0, (struct sockaddr*) &serveraddr,
    //                    // flags   where to send
    //                       sizeof(serveraddr));
    // if (did_send < 0) return errno;

    /* 4. Create buffer to store incoming data */
    char server_buf[BUF_SIZE];
    socklen_t serversize = sizeof(socklen_t); // Temp buffer for recvfrom API

    // socket non-blocking
    int out_flags = fcntl(sockfd, F_GETFL) | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, out_flags);
    // standard input non-blocking
    int inp_flags = fcntl(0, F_GETFL) | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, inp_flags);

    while (1)
    {
        /* 5. Listen for response from server */
        int bytes_recvd = recvfrom(sockfd, server_buf, BUF_SIZE,
                                   // socket  store data  how much
                                   0, (struct sockaddr *)&serveraddr,
                                   &serversize);
        // if data has been received
        if (bytes_recvd > 0)
        {
            write(1, server_buf, bytes_recvd);
        }

        int inp_size = read(0, client_buf, BUF_SIZE);
        /* 3. send request to the server if receive input*/
        if (inp_size > 0)
        {
            int did_send = sendto(sockfd, client_buf, inp_size,
                                  // socket  send data   how much to send
                                  0, (struct sockaddr *)&serveraddr,
                                  // flags   where to send
                                  sizeof(serveraddr));
            if (did_send < 0)
                return errno;
        }
    }

    /* 6. You're done! Terminate the connection */
    close(sockfd);
    return 0;
}
