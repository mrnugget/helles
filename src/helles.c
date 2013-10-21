#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "helles.h"
#include "logging.h"
#include "networking.h"

void trap_sig(int sig, void (*sig_handler)(int))
{
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

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
        kill(workerpids[i], SIGTERM);
    }
    free(workerpids);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        exit(1);
    }

    char *port = argv[1];
    int socket, i;

    // Listen on port
    if ((socket = he_listen(port)) < 0) {
        fprintf(stderr, "listen failed");
        exit(1);
    }

    // Pre-Fork Children
    workerpids = calloc(N_WORKERS, sizeof(pid_t));
    for (i = 0; i < N_WORKERS; i++) {
        int pid = fork();
        if (pid == 0) {
            // Child process
            he_accept(socket);
        } else if (pid > 0) {
            // Parent process
            workerpids[i] = pid;
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

    // Wait for all children
    while(wait(NULL) > 0);
    exit(0);
}
