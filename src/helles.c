#include <stdio.h>

#include "logging.h"
#include "networking.h"

int main(int argc, char *argv[])
{
    char *port = argv[1];
    int socket;

    if ((socket = he_listen(port)) < 0) {
        fprintf(stderr, "listen failed");
    }

    printf("Listening on port %s...\n", port);

    he_accept(socket);
}
