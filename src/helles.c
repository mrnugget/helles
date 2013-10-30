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

int send_fd(int socket, int *fd)
{
    // See: http://keithp.com/blogs/fd-passing/
    // See: libancillary
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    struct iovec nothing_ptr;

    struct {
        struct cmsghdr h;
        int fd[1];
    } buffer;

    char nothing = '!';

    nothing_ptr.iov_base = &nothing;
    nothing_ptr.iov_len = 1;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &nothing_ptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = &buffer;
    msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);

    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *((int *)CMSG_DATA(cmsg)) = *fd;

    return(sendmsg(socket, &msghdr, 0) >= 0 ? 0 : -1);
}

int recv_fd(int socket, int *fd)
{
    // See: http://keithp.com/blogs/fd-passing/
    // See: libancillary
    struct msghdr msghdr;
    struct cmsghdr *cmsg;
    struct iovec nothing_ptr;

    struct {
        struct cmsghdr h;
        int fd[1];
    } buffer;

    char nothing;

    nothing_ptr.iov_base = &nothing;
    nothing_ptr.iov_len = 1;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &nothing_ptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = &buffer;
    msghdr.msg_controllen = sizeof(struct cmsghdr) + sizeof(int);

    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    *(int *)CMSG_DATA(cmsg) = -1;

    if (recvmsg(socket, &msghdr, 0) < 0) {
        return -1;
    } else {
        *fd = *(int *)CMSG_DATA(cmsg);
        return 0;
    }
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
        int pid, sockfd[2], recvd_conn_fd, ipc_rc;

        socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);

        pid = fork();
        if (pid == 0) {
            // Child process
            close(listen_fd); // Worker does not need this
            close(sockfd[0]); // Close the 'parent-end' of the pipe

            for (;;) {
                // 1. Check sockfd[1] for new connection fd
                printf("[Worker %d] Waiting for new connection\n",
                        getpid());

                if (recv_fd(sockfd[1], &recvd_conn_fd) < 0) {
                    fprintf(stderr, "Could not receive recvd_conn_fd\n");
                    exit(1);
                }
                printf("[Worker %d] Received new connection: %d\n",
                        getpid(), recvd_conn_fd);

                handle_conn(recvd_conn_fd);

                printf("[Worker %d] Done\n", getpid());
                // 3. Write to sockfd[1] that we're ready again
                if ((ipc_rc = write(sockfd[1], "", 1)) != 1) {
                    fprintf(stderr, "Could write available-signal to socket\n");
                    exit(1);
                }
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
            if (i == N_WORKERS) {
                fprintf(stderr, "No available worker found\n");
                for (i = 0; i < N_WORKERS; i++) {
                    kill(workers[i].pid, SIGTERM);
                }
                exit(1);
            }

            // 3. Mark child as not available
            workers[i].available = 0;

            // 4. Send conn_fd to child
            printf("[Master] Sending connection to Worker %d\n", workers[i].pid);

            if (send_fd(workers[i].pipefd, &conn_fd) < 0) {
                fprintf(stderr, "Could not send conn_fd to worker\n");
            }

            printf("[Master] Sent to Worker %d\n", workers[i].pid);

            // 5. Close conn_fd here
            close(conn_fd);

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
