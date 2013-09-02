#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "networking.h"

int he_socket(char *port)
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

    freeaddrinfo(res);
    return sockfd;
}

int he_listen(int sockfd)
{
    int new_fd;
    socklen_t sin_size;
    struct sockaddr_storage client_addr;

    if (listen(sockfd, N_BACKLOG) == -1) {
        perror("listen");
        return -1;
    }

    while (1) {
        sin_size = sizeof(client_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if(!fork()) {
            close(sockfd);
            if (send(new_fd, "Hello, world!", 13, 0) == -1) {
                perror("send");
            }
            close(new_fd);
            return 0;
        }

        close(new_fd);
    }

    return 0;
}
