#ifndef _helles_main_h
#define _helles_main_h

#include "worker.h"

#define N_WORKERS 5

static struct worker *workers;

void trap_sig(int sig, void (*sig_handler)(int));
void sigint_handler(int s);

void err_kill_exit(char *msg);

int send_conn_worker(int n, struct worker *workers, int last_used, int fd);
int available_worker(int n, struct worker *workers, int last_used);
void kill_workers(int n, struct worker *workers);
int spawn_workers(int n, struct worker *workers, int listen_fd);


#endif
