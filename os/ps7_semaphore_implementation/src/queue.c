#include "queue.h"

int init_queue(struct queue *q)
{
  q->push = &_push;
  q->pop = &_pop;
  q->is_empty = &_is_empty;
  q->is_full = &_is_full;

  q->head = q->tail = -1;
  q->filled = 0;
}

void _push(struct queue *q, int data)
{
  q->tail++;
  q->q_array[q->tail % MAX_Q_SIZE] = data;
  q->filled++;
}

int _pop(struct queue *q)
{
  q->head++;
  int retval = q->q_array[q->head % MAX_Q_SIZE];
  q->filled--;
  return retval;
}

int _is_empty(struct queue *q)
{
  return (q->filled == 0);
}

int _is_full(struct queue *q)
{
  return (q->filled == MAX_Q_SIZE);
}

