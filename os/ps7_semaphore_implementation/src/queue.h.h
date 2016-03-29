#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>

#define MAX_Q_SIZE 64

struct queue {
  int q_array[MAX_Q_SIZE];
  int head;
  int tail;
  int filled;

  void (*push)(struct queue *q, int data);
  int (*pop)(struct queue *q);
  int (*is_empty)(struct queue *q);
  int (*is_full)(struct queue *q);
};

int init_queue(struct queue *q);
void _push(struct queue *q, int data);
int _pop(struct queue *q);
int _is_empty(struct queue *q);
int _is_full(struct queue *q);

#endif //QUEUE_H
