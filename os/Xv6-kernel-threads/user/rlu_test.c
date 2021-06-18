#include "rlu.h"
#include "user.h"

#define NUM_THREAD (35)
#define INIT_FOO(FOO_P, A)								\
  (FOO_P)->a = A;										\
  (FOO_P)->freed = 0

#define FREED_CHECK(FOO_P)\
  (FOO_P)->freed = 1


/* https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html
   from kernel.org What is RCU? article.
   modified rcu example into rlu example. */

struct foo {
  int a;
  short freed;
};

struct foo *global_foo;
lock_t ml;

struct foo *
rlu_new_node(void)
{
  lock_acquire(&ml);
  struct foo *p_new_foo = (struct foo *)RLU_ALLOC(sizeof(struct foo));
  lock_release(&ml);
  if (p_new_foo == NULL)
	{
	  printf(1, "out of memory\n");
	  exit();
	}

  return p_new_foo;
}

void foo_update_a(rlu_thread_data_t *self, int new_a)
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
  lock_acquire(&ml);
  RLU_FREE(self, old_fp);
  lock_release(&ml);
  FREED_CHECK(old_fp);

  RLU_READER_UNLOCK(self);
}

int foo_get_a(rlu_thread_data_t *self)
{
  int retval;

  RLU_READER_LOCK(self);
  retval = ((struct foo *)RLU_DEREF(self, global_foo))->a;
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
  int i;
  rlu_thread_data_t self;
  RLU_THREAD_INIT(&self);

  for (i = 0; i < 100; ++i)
	{
	  printf(1, "[%d] a: %d\n", getpid(), foo_get_a(&self));
	  if (i % 5 == 0)
		{
		  foo_update_a(&self, i*getpid());
		}
	}

  exit();
}

int main(int argc, char *argv[])
{
  int i;

  RLU_INIT();
  lock_init(&ml);
  init_global_foo(34);

  for (i = 0; i < NUM_THREAD; ++i)
	thread_create(&worker, NULL);

  for (i = 0; i < NUM_THREAD; ++i)
	thread_join();

  printf(1, "the final a: %d\n", global_foo->a);
  
  exit();
}
