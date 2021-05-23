#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "sysfunc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_clone(void)
{
  return clone();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_join(void)
{
  return join();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// rcu rapper
int
sys_rcu_init(void)
{
  int num_threads;
  
  if (argint(0, &num_threads) < 0)
	return -1;

  /* call implement here! */

  /* just for debugging */
  return num_threads;
}

int
sys_rcu_reader_lock(void)
{
  /* call implement here! */

  /* just for debugging */
  return 1234;
}

int
sys_rcu_reader_unlock(void)
{
  /* call implement here! */

  /* just for debugging */
  return 1234;
}

int
sys_rcu_writer_lock(void)
{
  int lock_id;
  
  if (argint(0, &lock_id) < 0)
	return -1;

  /* call implement here! */

  /* just for debugging */
  return lock_id;
}

int
sys_rcu_writer_unlock(void)
{
  int lock_id;
  
  if (argint(0, &lock_id) < 0)
	return -1;

  /* call implement here! */

  /* just for debugging */
  return lock_id;
}

int
sys_rcu_synchronize(void)
{
  /* call implement here! */

  /* just for debugging */
  return 1234;
}

int
sys_rcu_register(int id)
{
  int id;
  
  if (argint(0, &id) < 0)
	return -1;

  /* call implement here! */

  /* just for debugging */
  return id;
}

int
sys_rcu_unregister(void)
{
  /* call implement here! */

  /* just for debugging */
  return 1234;
}

int
sys_rcu_free(void)
{
  void *ptr;
  
  if (argint(0, &ptr) < 0)
	return -1;

  /* call implement here! */

  /* just for debugging */
  return ptr;
}
