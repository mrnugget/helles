#ifndef _helles_networking_h
#define _helles_networking_h

#define N_BACKLOG 128

int he_listen(char *port);
int accept_conn(int sockfd);

#endif
