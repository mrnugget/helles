#include "fmacros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"

static void handle_connection(int fd, char *buf, int bufn);

char *response_ok = "HTTP/1.1 200 OK\r\n"
"Content-Length: 32\r\n"
"Connection: close\r\n\r\n"
"It's a UNIX system! I know this!";

void worker_loop(int ipc_sock)
{
    int recvd_conn_fd, ipc_rc;
    char buffer[BUFSIZE];

    for (;;) {
        if (recv_fd(ipc_sock, &recvd_conn_fd) < 0) {
            fprintf(stderr, "Could not receive recvd_conn_fd\n");
            exit(1);
        }

        handle_connection(recvd_conn_fd, buffer, BUFSIZE);

        if ((ipc_rc = write(ipc_sock, "", 1)) != 1) {
            fprintf(stderr, "Could write available-signal to socket\n");
            exit(1);
        }
    }
}

static void handle_connection(int fd, char *buf, int bufsize)
{
    int nread;
    int response_len = strlen(response_ok);

    memset(buf, '\0', bufsize);

    do {
        if ((nread = recv(fd, buf, bufsize, 0)) < 0) {
            fprintf(stderr, "handle_connection: recv failed\n");
            break;
        }

        if (buf[nread-2] == '\r' && buf[nread-1] == '\n') {
            // Received a CRLF. Hacky, but works for responding with 200 OK.
            break;
        }
    } while (nread > 0);

    if (send(fd, response_ok, response_len, 0) != response_len) {
        fprintf(stderr, "handle_connection: send failed\n");
    }

    close(fd);
    return;
}
