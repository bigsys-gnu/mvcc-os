#include "pthread.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rlu.h"

#define NUM_THREAD (45)
#define INIT_FOO(FOO_P, A)								\
  (FOO_P)->a = A										

volatile unsigned int run;
pthread_barrier_t bar;

struct foo {
  int a;
};

volatile struct foo *global_foo;

struct foo *
rlu_new_node(void)
{
  struct foo *p_new_foo = (struct foo *)RLU_ALLOC(sizeof(struct foo));
  if (p_new_foo == NULL)
	{
	  printf("out of memory\n");
	  exit(1);
	}

  return p_new_foo;
}

void foo_update_a(rlu_thread_data_t *self, int new_a, int tid)
{
  struct foo *fp;

 restart:
  RLU_READER_LOCK(self);

  fp = (struct foo *) RLU_DEREF(self, global_foo);
  if (!RLU_TRY_LOCK(self, &fp))
	{
	  RLU_ABORT(self);

	  goto restart;
	}
  INIT_FOO(fp, new_a);

  RLU_READER_UNLOCK(self);
}

int foo_get_a(rlu_thread_data_t *self)
{
  int retval = 0, i;
  struct foo *ptr;

  RLU_READER_LOCK(self);
  ptr = (struct foo *)RLU_DEREF(self, global_foo);

  do {
  for (i = 0; i < 10; i++)
	if (i % 2 == 0)
	  retval++;
  }while(0);

  retval = ptr->a;

  RLU_READER_UNLOCK(self);

  return retval;
}

void init_global_foo(int init_a)
{
  global_foo = rlu_new_node();
  INIT_FOO(global_foo, init_a);
}

void *
worker(void *arg)
{
  int i, id;
  rlu_thread_data_t self;

  id = *(int *)arg;
  RLU_THREAD_INIT(&self);
  printf("please\n");

  pthread_barrier_wait(&bar);

  i = 0;
  while (run)
    {
	  printf("[%d] a: %d\n", id, foo_get_a(&self));
	  if (i % 13 == 0)
	    {
	      foo_update_a(&self, i*id, id);
	    }
      i++;
	}
  RLU_THREAD_FINISH(&self);
  return NULL;
}

int main(void)
{
  int i;
  int id_list[NUM_THREAD];
  pthread_t tid_list[NUM_THREAD];

  RLU_INIT();
  init_global_foo(34);
  pthread_barrier_init(&bar, 0, NUM_THREAD);
  run = 1;

  for (i = 0; i < NUM_THREAD; ++i)
	{
	  id_list[i] = i;
      xthread_create(&tid_list[i], 0, worker, &id_list[i]);
	}
  sleep(10);

  run = 0;
  for (i = 0; i < NUM_THREAD; ++i)
    pthread_join(tid_list[i], 0);

  RLU_PRINT_STATS();
  
  return 0;
}

