#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

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

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void sigint_handler(int s)
{
    printf("Terminating...\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    char *port = argv[1];
    int socket;

    trap_sig(SIGINT, sigint_handler);
    trap_sig(SIGCHLD, sigchld_handler);

    if ((socket = he_listen(port)) < 0) {
        fprintf(stderr, "listen failed");
    }

    printf("Listening on port %s...\n", port);

    he_accept(socket);
}
