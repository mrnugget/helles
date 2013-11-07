#include <stdlib.h>
#include <stdio.h>

#include "networking.h"
#include "ipc.h"
#include "worker.h"

void worker_loop(int ipc_sock)
{
    int recvd_conn_fd, ipc_rc;
    int pid = getpid();

    for (;;) {
        printf("[Worker %d] Waiting for new connection\n", pid);

        if (recv_fd(ipc_sock, &recvd_conn_fd) < 0) {
            fprintf(stderr, "Could not receive recvd_conn_fd\n");
            exit(1);
        }

        printf("[Worker %d] Received new connection: %d\n", pid, recvd_conn_fd);

        handle_conn(recvd_conn_fd);

        printf("[Worker %d] Done\n", pid);

        if ((ipc_rc = write(ipc_sock, "", 1)) != 1) {
            fprintf(stderr, "Could write available-signal to socket\n");
            exit(1);
        }
    }
}
