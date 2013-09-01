#include <stdio.h>

#include "logging.h"
#include "networking.h"

int main(int argc, char *argv[])
{
    if (he_boot(argv[1]) == -1) {
        fprintf(stderr, "Woah! Could not boot");
    }
}
