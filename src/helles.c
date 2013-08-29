#include <stdio.h>
#include <stdlib.h>
#include <logging.h>
#include <networking.h>

int main(int argc, char *argv[])
{
    LOG_GREEN("Hello World!\n");
    LOG_RED("Hello World!\n");
    setup_foo();
    return 0;
}
