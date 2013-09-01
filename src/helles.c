#include <stdio.h>

#include "logging.h"
#include "networking.h"

int main(int argc, char *argv[])
{
    int socket;
    if ((socket = he_setup_socket(argv[1])) == -1) {
        fprintf(stderr, "Woah! Could not setup");
    }

    if (he_listen(socket) == -1) {
        fprintf(stderr, "Woah! Could not listen");
    }
}
