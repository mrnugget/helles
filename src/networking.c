#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "networking.h"

#define N_BACKLOG 10

char response[] = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<html>"
"<head><title>Bye-bye baby bye-bye</title>"
"</head>"
"<body><h1>Goodbye, world!</h1></body></html>\r\n";

int he_listen(char *port)
{
    int sockfd, status;
    const int yes = 1;
    struct addrinfo hints, *res, *rptr;

    // Make sure the struct is zeroed before using it
    memset(&hints, 0, sizeof(hints));

    // Specify what we want
    hints.ai_family = AF_UNSPEC;     // Use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Automatically fill in the IP

    // Fill `res` linked list via getaddrinfo, based on the provided hints
    if ((status = getaddrinfo(NULL, port, &hints, &res) != 0)) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return status;
    }

    // Walk through the linked list `res` and try `socket(2)` on each node
    for(rptr = res; rptr != NULL; rptr = rptr->ai_next) {
        // Create a socket
        sockfd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol);
        if (sockfd < 0) {
            perror("he_socket: socket");
            continue;                 // Try with next node
        }

        // Enable SO_REUSEADDR
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                    sizeof(yes)) < 0) {
            perror("he_socket: setsockopt");
            continue;                // Try with next node
        };

        // Bind the socket
        if (bind(sockfd, rptr->ai_addr, rptr->ai_addrlen) < 0) {
            close(sockfd);
            perror("he_socket: bind");
            continue;                 // Try with next node
        }

        // socket, setsockopt and bind worked, jump out of the loop
        break;
    }

    if (res == NULL) {
        // We went through the loop without success
        return -1;
    }

    if (listen(sockfd, N_BACKLOG) < 0) {
        perror("he_socket: listen");
        return -1;
    }

    freeaddrinfo(res);
    return sockfd;
}

void handle_conn(int client_fd, char *buffer, int bufsize)
{
    int rc;

    if ((rc = recv(client_fd, buffer, bufsize, 0)) == -1) {
        fprintf(stderr, "Error reading from client\n");
        close(client_fd);
        return;
    }

    if (rc == 0) {
        printf("Client closed the connection");
        close(client_fd);
        return;
    }

    buffer[bufsize - 1] = '\0';

#ifdef DEBUG
    printf("[%d] Received: %d bytes\n", getpid(), rc);
    printf("%s\n", buffer);
#endif

    if (send(client_fd, response, sizeof(response), 0) == -1) {
        perror("accept_conn: send");
    }

    close(client_fd);
}

int accept_conn(int sockfd)
{
    int client_fd;
    socklen_t sin_size;
    struct sockaddr_storage client_addr;

    sin_size = sizeof(client_addr);
    client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    if (client_fd == -1) {
        fprintf(stderr, "could not accept");
        return -1;
    }

    return client_fd;
}
