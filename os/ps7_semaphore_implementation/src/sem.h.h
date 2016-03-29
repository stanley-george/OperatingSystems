#ifndef _SEM_H
#define _SEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>

#define N_PROC   64
#define LOCKED   1
#define UNLOCKED 0
#define SEM_BUFSIZE 3

#include "queue.h"

extern int my_procnum;

struct sem {
  char sem_lck;
  int  sem_cnt;
  struct queue sem_tskq;
  struct queue sem_pidq;
};

/*
 * TAS spin lock functions
 */
extern int tas(volatile char *lock);
int acquire_lock(volatile char *lock);
void release_lock(volatile char *lock);

/*
 * semaphore functions
 */
void sem_init(struct sem *s, int count);
int sem_try(struct sem *s);
void sem_wait(struct sem *s);
void sem_inc(struct sem *s);
void sem_sighandler(int signum);

#endif // _SEM_H

