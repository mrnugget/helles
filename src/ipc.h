#ifndef _helles_ipc_h
#define _helles_ipc_h

int send_fd(int socket, int *fd);
int recv_fd(int socket, int *fd);

#endif
