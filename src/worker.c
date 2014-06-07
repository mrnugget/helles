#include "fmacros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"

static void handle_connection(int fd, char *buf, int bufn);

char *response_ok =
"HTTP/1.1 200 OK\n"
"Server: Helles 0.0.1\n"
"Content-Type: text/plain\n"
"Content-Length: 33\n"
"\n"
"It's a unix system! I know this!\n";

void worker_loop(int ipc_sock)
{
    int recvd_conn_fd, ipc_rc;
    char buffer[BUFSIZE];

#ifdef DEBUG
    int pid = getpid();
#endif

    for (;;) {
#ifdef DEBUG
        printf("[Worker %d] Waiting for new connection\n", pid);
#endif

        if (recv_fd(ipc_sock, &recvd_conn_fd) < 0) {
            fprintf(stderr, "Could not receive recvd_conn_fd\n");
            exit(1);
        }

#ifdef DEBUG
        printf("[Worker %d] Received new connection: %d\n", pid, recvd_conn_fd);
#endif

        handle_connection(recvd_conn_fd, buffer, BUFSIZE);

#ifdef DEBUG
        printf("[Worker %d] Done\n", pid);
#endif

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

    do {
        if ((nread = recv(fd, buf, bufsize, 0)) < 0) {
            fprintf(stderr, "handle_connection: recv failed\n");
            break;
        }

        if (buf[nread-2] == '\r' && buf[nread-1] == '\n') {
            // Received a CRLF. Hacky, but works for responding with 200 OK.
            break;
        }

#ifdef DEBUG
        printf("%s", buf);
#endif
    } while (nread > 0);

    if (send(fd, response_ok, response_len, 0) != response_len) {
        fprintf(stderr, "handle_connection: send failed\n");
    }

    close(fd);
    return;
}
