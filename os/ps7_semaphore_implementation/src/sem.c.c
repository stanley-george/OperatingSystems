#include "sem.h"


int acquire_lock(volatile char *lock)
{ while(tas(lock) == LOCKED) ; return UNLOCKED; }


void release_lock(volatile char *lock)
{ *lock = UNLOCKED; }


void sem_init(struct sem *s, int count)
{
  init_queue(&(s->sem_tskq));
  init_queue(&(s->sem_pidq));
  s->sem_cnt = count;
  s->sem_lck = UNLOCKED;
  signal(SIGUSR1, sem_sighandler);
}

int sem_try(struct sem *s)
{
  sigset_t oldmask, newmask;
  sigfillset(&newmask);
  sigprocmask(SIG_BLOCK, &newmask, &oldmask);
  if(acquire_lock(&(s->sem_lck)) == UNLOCKED)
  {
    int retval;
    if(s->sem_cnt > 0)
    {
      s->sem_cnt -= 1;
      retval = 1;
    }
    else
      retval = 0;
    release_lock(&(s->sem_lck));
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return retval;
  }
}

void sem_wait(struct sem *s)
{
  sigset_t oldmask, newmask;
  sigfillset(&newmask);
  sigprocmask(SIG_BLOCK, &newmask, &oldmask);
  while(1)
  {
    if(acquire_lock(&(s->sem_lck)) == UNLOCKED)
    {
      if(s->sem_cnt > 0)
      {
        s->sem_cnt -= 1;
        sigprocmask(SIG_BLOCK, &oldmask, NULL);
        release_lock(&(s->sem_lck));
        break;
      }
      else
      {
        s->sem_tskq.push(&(s->sem_tskq), my_procnum);
        s->sem_pidq.push(&(s->sem_pidq), getpid());
        sigset_t proc_sigmask;
        sigfillset(&proc_sigmask);
        sigdelset(&proc_sigmask, SIGUSR1);
        release_lock(&(s->sem_lck));
        sigsuspend(&proc_sigmask);
      }
    }
  }
}

void sem_inc(struct sem *s)
{
  sigset_t oldmask, newmask;
  sigfillset(&newmask);
  sigprocmask(SIG_BLOCK, &newmask, &oldmask);
  if(acquire_lock(&(s->sem_lck)) == UNLOCKED)
  {
    s->sem_cnt += 1;
    /*while there are blocked processes, wake them up*/
    while(s->sem_tskq.filled != 0)
    {
      int vid = s->sem_tskq.pop(&(s->sem_tskq));
      pid_t pid = s->sem_pidq.pop(&(s->sem_pidq));
      kill(pid, SIGUSR1);
    }
    sigprocmask(SIG_BLOCK, &oldmask, NULL);
    release_lock(&(s->sem_lck));
  }
}

void sem_sighandler(int signum)
{
  if(signum == SIGUSR1) ;
}






















