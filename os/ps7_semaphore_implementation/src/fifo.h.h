#ifndef FIFO_H
#define FIFO_H

#define MYFIFO_BUFSIZ 100

struct fifo {
  struct sem *sem_rd;
  struct sem *sem_wr;
  struct sem *sem_fifo;

  int head;
  int tail;
  int filled;
  unsigned long buf[MYFIFO_BUFSIZ];

  void (*fifo_push)(struct fifo *f, unsigned long data);
  unsigned long (*fifo_pop)(struct fifo *f);
  int (*fifo_is_empty)(struct fifo *f);
  int (*fifo_is_full)(struct fifo *f);
};

void fifo_init(struct fifo *f);
void fifo_wr(struct fifo *f, unsigned long d);
unsigned long fifo_rd(struct fifo *f);

void _fifo_push(struct fifo *f, unsigned long data);
unsigned long _fifo_pop(struct fifo *f);
int _fifo_is_empty(struct fifo *f);
int _fifo_is_full(struct fifo *f);

#endif // FIFO_H
