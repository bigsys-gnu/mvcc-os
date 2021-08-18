/* problematic solution */
void*
send(struct q *q, void *p) {
  acquire(&q->lock);
  while(q->ptr != 0);
  q->ptr = p;
  wakeup(q);
  release(&q->lock);
}

void*
recv(struct q *q) {
  void *p;

  acquire(&q->lock);
  while((p = q->ptr) == 0)
	sleep(q);
  q->ptr = 0;
  release(&q->lock);
  return p;
}

/* good solution */
void*
send(struct q *q, void *p) {
  acquire(&q->lock);
  while(q->ptr != 0);
  q->ptr = p;
  wakeup(q);
  release(&q->lock);
}

void*
recv(struct q *q) {
  void *p;

  acquire(&q->lock);
  while((p = q->ptr) == 0)
	sleep(q, &q->lock);
  q->ptr = 0;
  release(&q->lock);
  return p;
}

/* deadlock version */
void*
send(struct q *q, void *p) {
  while(q->ptr != 0);
  q->ptr = p;
  wakeup(q);
}

void*
recv(struct q *q) {
  void *p;

  while((p = q->ptr) == 0)
	sleep(q);
  q->ptr = 0;
  return p;
}
