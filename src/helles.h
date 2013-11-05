#ifndef _helles_helles_h
#define _helles_helles_h

#define N_WORKERS 5

struct worker {
  pid_t pid;
  int pipefd;
  int available;
  int count;
};

static struct worker *workers;

#endif
