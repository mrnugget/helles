#include <stdio.h>

#include "logging.h"
#include "networking.h"

int main(int argc, char *argv[])
{
    char *port = argv[1];
    int socket;

    if ((socket = he_socket(port)) != 0) {
        fprintf(stderr, "Woah! Could not setup");
    }

    if (he_listen(socket) == -1) {
        fprintf(stderr, "Woah! Could not listen");
    }
}
