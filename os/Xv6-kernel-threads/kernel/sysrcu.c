#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "krcu.h"
#include "proc.h"
#include "sysfunc.h"

// rcu rapper
int
sys_rcu_init(void)
{
  struct proc *parent = proc->parent;
  int num_threads;
  
  if (argint(0, &num_threads) < 0)
	return -1;
  /* call implement here! */
  rcu_init(&parent->rm, num_threads);
  /* just for debugging */
  return num_threads;
}

int
sys_rcu_reader_lock(void)
{
  struct proc *parent = proc->parent;
  struct rcu_data *d = NULL;
  int tmp;

  if (argint(0, &tmp) < 0)
	return -1;

  d = (struct rcu_data *)tmp;

  /* call implement here! */
  rcu_reader_lock(&parent->rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_reader_unlock(void)
{
  struct proc *parent = proc->parent;
  struct rcu_data *d = NULL;
  int tmp;

  if (argint(0, &tmp) < 0)
	return -1;

  d = (struct rcu_data *)tmp;

  /* call implement here! */
  rcu_reader_unlock(&parent->rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_writer_lock(void)
{
  struct proc *parent = proc->parent;
  int lock_id;
  
  if (argint(0, &lock_id) < 0)
	return -1;

  /* call implement here! */
  rcu_writer_lock(&parent->rm, lock_id);
  /* just for debugging */
  return 1;
}

int
sys_rcu_writer_unlock(void)
{
  struct proc *parent = proc->parent;
  int lock_id;
  
  if (argint(0, &lock_id) < 0)
	return -1;

  /* call implement here! */
  rcu_writer_unlock(&parent->rm, lock_id);
  /* just for debugging */
  return 1;
}

int
sys_rcu_synchronize(void)
{
  int tmp;
  struct rcu_data *d;
  struct proc *parent = proc->parent;

  if (argint(0, &tmp) < 0)
	return -1;

  d = (struct rcu_data *)tmp;
  /* call implement here! */
  rcu_synchronize(&parent->rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_register(void)
{
  int tmp;
  struct rcu_data *d;
  struct proc *parent = proc->parent;

  if (argint(0, &tmp) < 0)
	return -1;

  d = (struct rcu_data *)tmp;
  /* call implement here! */
  rcu_register(&parent->rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_unregister(void)
{
  int tmp;
  struct rcu_data *d;

  if (argint(0, &tmp) < 0)
	return -1;

  d = (struct rcu_data *)tmp;

  /* call implement here! */
  rcu_unregister(d);
  /* just for debugging */
  return 1;
}

/* get int parameter as a casted pointer value. */
int
sys_rcu_free(void)
{
  int tmp0, tmp1;
  struct proc *parent = proc->parent;
  struct rcu_data *d;
  void *ptr;

  if (argint(0, &tmp0) < 0)
	return -1;
  
  if (argint(1, &tmp1) < 0)
	return -1;

  d = (struct rcu_data *)tmp0;
  ptr = (void *)tmp1;

  /* call implement here! */
  rcu_free(&parent->rm, d, ptr);
  /* just for debugging */
  return 1;
}
