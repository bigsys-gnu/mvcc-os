/* Test new rcu syscall
   rcu call has no implementation.
*/

#include "types.h"
#include "user.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "x86.h"
#include "rcu.h"

#define THREAD_NUM 35

uint total_write;
uint per_thread_write[THREAD_NUM];
int sanity_failed;

#define COUNT_WRITE(ID)\
  __sync_add_and_fetch(&total_write, 1); \
  per_thread_write[ID]++

#define SEGMENTATION_FAULT_CHECK(FOO_PTR)\
  if (FOO_PTR->freed) \
	__sync_add_and_fetch(&sanity_failed, 1)

#define FREE_FOO(FOO_PTR)\
	FOO_PTR->freed = 1;	 \
	th_free(FOO_PTR);

struct rcu_maintain rm;
lock_t lk;						/* rcu write lock */


/* rcu example from https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#id6 */

struct foo {
  int a;
  int freed;
};

struct foo *global_foo;

struct foo *
create_foo(int new_a)
{
  struct foo *new_fp = (struct foo *) th_malloc(sizeof(struct foo));

  if (new_fp)
	{
	  new_fp->a = new_a;
	  new_fp->freed = 0;
	}
  return new_fp;
}

void foo_update_a(struct rcu_data *rd, int new_a)
{
  struct foo *new_fp;
  struct foo *old_fp;

  new_fp = create_foo(new_a);
  lock_acquire(&lk);
  old_fp = global_foo;
  global_foo = new_fp;
  lock_release(&lk);
  rcu_synchronize(&rm, rd);
  FREE_FOO(old_fp);             /* old_fp is freed */
  COUNT_WRITE(rd->id);			/* for sanity_failed check */
}

int foo_get_a(struct rcu_data *rd)
{
  int retval = 0;
  struct foo *ptr;
  int i;

  rcu_reader_lock(&rm, rd);
  ptr = global_foo;
  for (i = 0; i < 100000; ++i)
	retval++; /* waiting with doing a meaningless thing. */

  SEGMENTATION_FAULT_CHECK(ptr); /* The error should be occurred when the stale global_foo got freed. */

  retval = ptr->a;
  rcu_reader_unlock(&rm, rd);
  return retval;
}

void worker(void *ptr)
{
  struct rcu_data rd;
  int i;

  rd.id = *(int *)ptr;
  printf(1, "id: %d\n", rd.id);
  rcu_register(&rm, &rd);

  for (i = 0; i < 30; ++i)
	{
	  printf(1, "thread %d: val: %d\n", getpid(), foo_get_a(&rd));
	  foo_update_a(&rd, i*rd.id);
	}
  rcu_unregister(&rd);

  exit();
}

void
sanity_init(void)
{
  int i;

  total_write = 0;
  sanity_failed = 0;

  for (i = 0; i < THREAD_NUM; ++i)
	per_thread_write[i] = 0;
}

int
sanity_check(void)
{
  int sum = 0;
  int i;

  for (i = 0; i < THREAD_NUM; ++i)
	sum += per_thread_write[i];

  printf(1,
		 "sanity_failed check result\n"
		 "total_write: %d\n"
		 "sum_write: %d\n"
		 "sanity_failed failed: %d\n",
		 total_write,
		 sum,
		 sanity_failed);

  return (total_write == sum);
}

int
main(void)
{
  int i;
  int tid_list[THREAD_NUM];

  sanity_init();
  rcu_init(&rm, THREAD_NUM);
  printf(1, "on user rm: %p\n", &rm);
  lock_init(&lk);
  global_foo = (struct foo *) th_malloc(sizeof(struct foo));
  global_foo->a = 1;
  global_foo->freed = 0;

  for (i = 0; i < THREAD_NUM; ++i)
	{
	  tid_list[i] = i;
	  thread_create(worker, &tid_list[i]);
	}

  for (i = 0; i < THREAD_NUM; ++i)
	thread_join();

  sanity_check();
  exit();
}
