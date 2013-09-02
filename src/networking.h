#ifndef _helles_networking_h
#define _helles_networking_h

#define N_BACKLOG 10

int he_listen(char *port);
int he_accept(int sockfd);

#endif
