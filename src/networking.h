#ifndef _helles_networking_h
#define _helles_networking_h

#define N_BACKLOG 10

int he_setup_socket(char *port);
int he_listen(int sockfd);

#endif
