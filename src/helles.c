#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include "helles.h"
#include "logging.h"
#include "networking.h"

void trap_sig(int sig, void (*sig_handler)(int))
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;
#ifdef SA_RESTART
    sa.sa_flags |= SA_RESTART;
#endif

    if (sigaction(sig, &sa, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
}

void sigint_handler(int s)
{
    int i;

    printf("Terminating...\n");

    for (i = 0; i < N_WORKERS; i++) {
        kill(workers[i].pid, SIGTERM);
    }

    free(workers);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        exit(1);
    }

    char *port = argv[1];
    int listen_fd, conn_fd;
    int i, maxfd, sc;
    int ipc_buf, ipc_rc;
    fd_set readset, masterset;

    // Listen on port
    if ((listen_fd = he_listen(port)) < 0) {
        fprintf(stderr, "listen failed");
        exit(1);
    }

    FD_ZERO(&masterset);
    FD_SET(listen_fd, &masterset);
    maxfd = listen_fd;

    // Pre-Fork workers
    workers = calloc(N_WORKERS, sizeof(struct worker));
    for (i = 0; i < N_WORKERS; i++) {
        int pid, sockfd[2];

        socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);

        pid = fork();
        if (pid == 0) {
            // Child process
            close(listen_fd);     // Worker does not need this
            close(sockfd[0]);         // Close the 'parent-end' of the pipe
            // TODO: do work here (wait for new connection, handle it)
            for (;;) {
                // 1. Check sockfd[1] for new connection fd
                // @TODO: implement `recv_fd`
                // 2. Handle the connection
                // 3. Write to sockfd[1] that we're ready again
                sleep(1);
            }
        } else if (pid > 0) {
            // Parent process
            close(sockfd[1]);
            workers[i].pid = pid;
            workers[i].available = 1;
            workers[i].pipefd = sockfd[0];
            FD_SET(workers[i].pipefd, &masterset); // Add the worker pipe to set
            maxfd = workers[i].pipefd > maxfd ? workers[i].pipefd : maxfd;
        } else {
            // Something went wrong while forking
            fprintf(stderr, "fork failed");
            exit(1);
        }
    }

    // Trap signals
    trap_sig(SIGINT, sigint_handler);

    printf("Helles booted up. %d workers listening on port %s\n",
            N_WORKERS, port);

    for ( ; ; ) {
        readset = masterset;
        if ((sc = select(maxfd + 1, &readset, NULL, NULL, NULL)) < 0) {
            fprintf(stderr, "select failed");
            exit(1);
        }

        // Check if new connection needs to be accepted
        if (FD_ISSET(listen_fd, &readset)) {
            // Accept the new connection
            if ((conn_fd = accept_conn(listen_fd)) < 0) {
                exit(1);
            }

            // 1. Go through all the children
            for (i = 0; i < N_WORKERS; i++) {
                // 2. Check which one is available
                if (workers[i].available) {
                    // This one is available, jump out of loop, remember i
                    break;
                }
            }
            // 3. Mark child as not available
            workers[i].available = 0;

            // 4. Send conn_fd to child
            // @TODO: implement `send_fd`

            // 5. Close conn_fd here
            handle_conn(conn_fd);

            // If nothing else is readable, jump back to select()
            if (--sc == 0) {
                continue;
            }
        }

        // Check if message from worker is ready to read
        for (i = 0; i < N_WORKERS; i++) {
            if (FD_ISSET(workers[i].pipefd, &readset)) {
                if ((ipc_rc = read(workers[i].pipefd, &ipc_buf, 1)) == 0) {
                    fprintf(stderr, "Could not read from worker socket");
                    exit(1);
                }
                workers[i].available = 1;

                // No workers ready to read anymore, no need to check them
                if (--sc == 0) {
                    break;
                }
            }
        }
    }

    return 0;
}
