#include "sem.h"
#include "fifo.h"

void fifo_init(struct fifo *f)
{
  if((f->sem_rd = (struct sem *) mmap(0, sizeof(struct sem), PROT_READ|PROT_WRITE,
    MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) { perror("map"); exit(1); }
  if((f->sem_wr = (struct sem *) mmap(0, sizeof(struct sem), PROT_READ|PROT_WRITE,
    MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) { perror("map"); exit(1); }
  if((f->sem_fifo = (struct sem *) mmap(0, sizeof(struct sem), PROT_READ|PROT_WRITE,
    MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) { perror("map"); exit(1); }

  sem_init(f->sem_rd, 0);
  sem_init(f->sem_wr, MYFIFO_BUFSIZ);
  sem_init(f->sem_fifo, 1);

  f->head = f->tail = -1;
  f->filled = 0;

  f->fifo_is_empty = &_fifo_is_empty;
  f->fifo_is_full = &_fifo_is_full;
  f->fifo_push = &_fifo_push;
  f->fifo_pop = &_fifo_pop;
}


void fifo_wr(struct fifo *f, unsigned long data)
{
  sem_wait(f->sem_fifo); /*attempt to obtain fifo access; sleep till success*/
  sem_wait(f->sem_wr);   /*attempt to obtain write access; sleep till success*/
  f->fifo_push(f, data); /*critical region; only 1 proccess at a time*/
  sem_inc(f->sem_fifo);  /*open fifo access to other processes*/
  sem_inc(f->sem_rd);    /*open read access to other processes*/
}

unsigned long fifo_rd(struct fifo *f)
{
	unsigned long retval;
	sem_wait(f->sem_fifo);   /*attempt to obtain fifo access; sleep till success*/
	sem_wait(f->sem_rd);     /*attempt to obtain read access; sleep till success*/
	retval = f->fifo_pop(f); /*critical region; only 1 proccess at a time*/
	sem_inc(f->sem_fifo);    /*open fifo access to other processes*/
	sem_inc(f->sem_wr);      /*open write access to other processes*/
	return retval;
}


void _fifo_push(struct fifo *f, unsigned long data)
{
  f->tail++;
  f->buf[f->tail % MYFIFO_BUFSIZ] = data;
  f->filled++;
}

unsigned long _fifo_pop(struct fifo *f)
{
  f->head++;
  unsigned long retval = f->buf[f->head % MYFIFO_BUFSIZ];
  f->filled--;
  return retval;
}

int _fifo_is_empty(struct fifo *f)
{
  return (f->filled == 0);
}

int _fifo_is_full(struct fifo *f)
{
  return (f->filled == MYFIFO_BUFSIZ);
}

