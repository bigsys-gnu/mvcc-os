#include "rlu.h"
#include "types.h"
#include "user.h"

#define NUM_THREAD (35)
#define INIT_FOO(FOO_P, A)								\
  (FOO_P)->a = A;										\
  (FOO_P)->freed = 0

#define FREED_CHECK(FOO_P)\
  (FOO_P)->freed = 1

#define COUNT_WRITE(ID)							\
  __sync_add_and_fetch(&total_write, 1);		\
  per_thread_write[ID]++

#define SEGMENTATION_FAULT_CHECK(FOO_PTR)		\
  if (FOO_PTR->freed)							\
	__sync_add_and_fetch(&sanity_failed, 1)

uint total_write;
uint per_thread_write[NUM_THREAD];
uint sanity_failed;


/* https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html
   from kernel.org What is RCU? article.
   modified rcu example into rlu example. */

struct foo {
  int a;
  short freed;
};

struct foo *global_foo;

struct foo *
rlu_new_node(void)
{
  struct foo *p_new_foo = (struct foo *)RLU_ALLOC(sizeof(struct foo));
  if (p_new_foo == NULL)
	{
	  printf(1, "out of memory\n");
	  exit();
	}

  return p_new_foo;
}

void foo_update_a(rlu_thread_data_t *self, int new_a, int tid)
{
  struct foo *new_fp;
  struct foo *old_fp;

  new_fp = rlu_new_node();
  INIT_FOO(new_fp, new_a);

 restart:
  RLU_READER_LOCK(self);

  old_fp = RLU_DEREF(self, global_foo);
  if (!RLU_TRY_LOCK(self, &old_fp))
	{
	  RLU_ABORT(self);
	  goto restart;
	}
  RLU_ASSIGN_PTR(self, &global_foo, new_fp);
  RLU_FREE(self, old_fp);
  FREED_CHECK(old_fp);

  RLU_READER_UNLOCK(self);
  
  COUNT_WRITE(tid);
}

int foo_get_a(rlu_thread_data_t *self)
{
  int retval = 0, i;
  struct foo *ptr;

  RLU_READER_LOCK(self);
  ptr = (struct foo *)RLU_DEREF(self, global_foo);

  SEGMENTATION_FAULT_CHECK(ptr);

  for (i = 0; i < 1000000; i++)
	if (i % 2 == 0)
	  retval++;

  retval = ptr->a;

  SEGMENTATION_FAULT_CHECK(ptr);

  RLU_READER_UNLOCK(self);

  return retval;
}

void init_global_foo(int init_a)
{
  global_foo = rlu_new_node();
  INIT_FOO(global_foo, init_a);
}

void worker(void *arg)
{
  int i, tid;
  rlu_thread_data_t self;

  tid = *(int *)arg;
  RLU_THREAD_INIT(&self);

  for (i = 0; i < 100; ++i)
	{
	  printf(1, "[%d] a: %d\n", getpid(), foo_get_a(&self));
	  if (i % 5 == 0)
		{
		  foo_update_a(&self, i*getpid(), tid);
		}
	}

  exit();
}

void
sanity_init(void)
{
  int i;

  total_write = 0;
  sanity_failed = 0;

  for (i = 0; i < NUM_THREAD; i++)
	per_thread_write[i] = 0;
}

void
sanity_check(void)
{
  int sum = 0;
  int i;

  for (i = 0; i < NUM_THREAD; i++)
	sum += per_thread_write[i];

  printf(1,
		 "sanity_failed check result\n"
		 "total_write: %d\n"
		 "sum_write: %d\n"
		 "sanity_failed failed: %d\n",
		 total_write,
		 sum,
		 sanity_failed);
}

int main(int argc, char *argv[])
{
  int i;
  int tid_list[NUM_THREAD];

  RLU_INIT();
  sanity_init();
  init_global_foo(34);

  for (i = 0; i < NUM_THREAD; ++i)
	{
	  tid_list[i] = i;
	  thread_create(&worker, &tid_list[i]);
	}

  for (i = 0; i < NUM_THREAD; ++i)
	thread_join();

  printf(1, "the final a: %d\n", global_foo->a);
  sanity_check();
  
  exit();
}
