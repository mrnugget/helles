#ifndef _helles_helles_h
#define _helles_helles_h

#define N_WORKERS 5

struct worker {
  pid_t pid;
};

static struct worker *workers;

#endif
