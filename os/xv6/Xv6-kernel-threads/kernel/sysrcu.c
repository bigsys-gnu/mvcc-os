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
  int tmp;
  int num_threads;
  struct rcu_maintain *rm;

  if (argint(0, &tmp) < 0)
	return -1;
  
  if (argint(1, &num_threads) < 0)
	return -1;

  rm = (struct rcu_maintain *)tmp;

  /* call implement here! */
  rcu_init(rm, num_threads);
  /* just for debugging */
  return num_threads;
}

int
sys_rcu_reader_lock(void)
{
  int tmp0, tmp1;
  struct rcu_maintain *rm;
  struct rcu_data *d;

  if (argint(0, &tmp0) < 0)
	return -1;
  
  if (argint(1, &tmp1) < 0)
	return -1;

  rm = (struct rcu_maintain *)tmp0;
  d = (struct rcu_data *)tmp1;

  /* call implement here! */
  rcu_reader_lock(rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_reader_unlock(void)
{
  int tmp0, tmp1;
  struct rcu_maintain *rm;
  struct rcu_data *d;

  if (argint(0, &tmp0) < 0)
	return -1;
  
  if (argint(1, &tmp1) < 0)
	return -1;

  rm = (struct rcu_maintain *)tmp0;
  d = (struct rcu_data *)tmp1;

  /* call implement here! */
  rcu_reader_unlock(rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_synchronize(void)
{
  int tmp0, tmp1;
  struct rcu_maintain *rm;
  struct rcu_data *d;

  if (argint(0, &tmp0) < 0)
	return -1;
  
  if (argint(1, &tmp1) < 0)
	return -1;

  rm = (struct rcu_maintain *)tmp0;
  d = (struct rcu_data *)tmp1;

  /* call implement here! */
  rcu_synchronize(rm, d);
  /* just for debugging */
  return 1;
}

int
sys_rcu_register(void)
{
  int tmp0, tmp1;
  struct rcu_maintain *rm;
  struct rcu_data *d;

  if (argint(0, &tmp0) < 0)
	return -1;
  
  if (argint(1, &tmp1) < 0)
	return -1;

  rm = (struct rcu_maintain *)tmp0;
  d = (struct rcu_data *)tmp1;

  /* call implement here! */
  rcu_register(rm, d);
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
