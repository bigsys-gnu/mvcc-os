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
/* #define LOCK_ID 0 */


struct rcu_maintain rm;
lock_t lk;


/* rcu example from https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html#id6 */

struct foo {
  int a;
  char b;
  long c;
};

struct foo *global_foo;

void foo_update_a(struct rcu_data *rd, int new_a)
{
  struct foo *new_fp;
  struct foo *old_fp;

  new_fp = (struct foo *) malloc(sizeof(*new_fp));
  /* rcu_writer_lock(&rm, LOCK_ID); */
  lock_acquire(&lk);
  old_fp = global_foo;
  *new_fp = *old_fp;
  new_fp->a = new_a;
  global_foo = new_fp;
  /* rcu_writer_unlock(&rm, LOCK_ID); */
  lock_release(&lk);
  rcu_synchronize(&rm, rd);
  free(old_fp);
}

int foo_get_a(struct rcu_data *rd)
{
  int retval;

  rcu_reader_lock(&rm, rd);
  retval = global_foo->a;
  printf(1, "foo_get_a %d\n", retval);
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
	  foo_update_a(&rd, i);
	}
  rcu_unregister(&rd);

  exit();
}

int
main(void)
{
  int i;
  int tid_list[THREAD_NUM];

  rcu_init(&rm, THREAD_NUM);
  printf(1, "on user rm: %p\n", &rm);
  lock_init(&lk);
  global_foo = (struct foo *) malloc(sizeof(struct foo));
  global_foo->a = 1;
  global_foo->b = 'a';
  global_foo->c = 2;

  for (i = 0; i < THREAD_NUM; ++i)
	{
	  tid_list[i] = i;
	  thread_create(worker, &tid_list[i]);
	}

  for (i = 0; i < THREAD_NUM; ++i)
	thread_join();
  
  exit();
}
