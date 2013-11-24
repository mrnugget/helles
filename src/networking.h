#ifndef _helles_networking_h
#define _helles_networking_h

int he_listen(char *port);
int accept_conn(int sockfd);
void handle_conn(int client_fd, char *buffer, int bufsize);

#endif
