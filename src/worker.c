#include "fmacros.h"
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"

static void echofd(int fd, char *buf, int bufn);

void worker_loop(int ipc_sock)
{
    int recvd_conn_fd, ipc_rc;
#ifdef DEBUG
    int pid = getpid();
#endif
    char buffer[BUFSIZE];

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

        echofd(recvd_conn_fd, buffer, BUFSIZE);
#ifdef DEBUG
        printf("[Worker %d] Done\n", pid);
#endif

        if ((ipc_rc = write(ipc_sock, "", 1)) != 1) {
            fprintf(stderr, "Could write available-signal to socket\n");
            exit(1);
        }
    }
}

static void echofd(int fd, char *buf, int bufsize)
{
    int nread;

    do {
        if ((nread = recv(fd, buf, bufsize, 0)) < 0) {
            fprintf(stderr, "echo: recv failed\n");
            break;
        }

#ifdef DEBUG
        printf("%s", buf);
#endif
        if (send(fd, buf, nread, 0) != nread) {
            fprintf(stderr, "echo: send failed\n");
            break;
        }
    } while (nread > 0);

    close(fd);
    return;
}
