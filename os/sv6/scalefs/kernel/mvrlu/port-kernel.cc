#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "vmalloc.hh"
#include "proc.hh"
#include "mvrlu/arch.h"
#include "mvrlu/port-kernel.h"

/*
 * Log region
 */
static unsigned long g_size __read_mostly;

int port_log_region_init(unsigned long size, unsigned long num)
{
  g_size = size;
  return 0;
}

void port_log_region_destroy(void)
{
  /* do nothing */
}

void *port_alloc_log_mem(void)
{
  return vmalloc_raw(g_size, 4, "port log");
}

void port_free_log_mem(void *addr)
{
  vmalloc_free(addr);
}

int port_addr_in_log_region(void *addr__)
{
  // unsigned long addr = (unsigned long)addr__;
  // return addr >= VMALLOC_START && addr < VMALLOC_END;
  return 1;                   // need fix
}

/*
 * Memory allocation
 */
struct alloc_mem {
  size_t size;
  char byte[1];
};

void *port_alloc_x(size_t size, unsigned int flags)
{
  struct alloc_mem *mem;

  kmalign((void **)&mem, 4, size + sizeof(size_t), "port mem");
  mem->size = size + sizeof(size_t);
  return mem->byte;
}

void port_free(void *ptr)
{
  int *int_ptr = (int *)ptr;
  struct alloc_mem *mem = (struct alloc_mem *)(int_ptr - sizeof(int *));
  kmalignfree(mem, 4, mem->size);
}

void port_cpu_relax_and_yield(void)
{
  yield();
  cpu_relax();
}

void port_spin_init(spinlock_t *lock)
{
  lock->spin_obj = (void *) new spinlock("port lock");
}

void port_spin_destroy(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;

  if (l)
    delete l;
}

void port_spin_lock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  l->acquire();
}

int port_spin_trylock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->try_acquire();
}

void port_spin_unlock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->release();
}

int port_mutex_init(struct mutex *mutex)
{
  mutex->mutex_obj = (void *) new spinlock("port mutex");
  return 1;
}

int port_mutex_destroy(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;

  if (l)
    delete l;

  return 1;
}

int port_mutex_lock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->acquire();
  return 1;
}

int port_mutex_unlock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->release();
  return 1;
}

void port_cond_init(struct completion *cond)
{
  cond->cond_obj = (void *) new condvar("port cond");
}

void port_cond_destroy(struct completion *cond)
{
  condvar *cond_var = (condvar *)cond->cond_obj;
  delete cond_var;
}

/*
 * Thread
 */

int port_create_thread(const char *name, struct task_struct **t,
                       void (*fn)(void *), void *arg,
                       struct completion *completion)
{
  struct proc *temp = threadalloc(fn, arg);
  if (temp != NULL)
  {
    port_cond_init(completion);

    {
      scoped_acquire l(&temp->lock);
      addrun(temp);
    }
    *t = (struct task_struct *)temp;
    return 0;
  }
  return -11;
}

void port_finish_thread(struct completion *completion)
{
  struct condvar *cond = (struct condvar *) completion->cond_obj;
  cond->wake_all();
}

void port_wait_for_finish(void *x, struct completion *completion)
{
  struct condvar *cond = (struct condvar *) completion->cond_obj;
  struct spinlock local_lock("local");

  scoped_acquire lg(&local_lock);
  cond->sleep(&local_lock);
}

void port_initiate_wakeup(struct mutex *mutex, struct completion *cond)
{
  struct condvar *cond_var = (struct condvar *) cond->cond_obj;
  (void)mutex;
  cond_var->wake_all();
}

void port_initiate_nap(struct mutex *mutex, struct completion *cond,
                       unsigned long usecs)
{
  struct condvar *cond_var = (struct condvar *) cond->cond_obj;

  delete cond_var;
  cond_var = new condvar("port cond");
  cond->cond_obj = (void *) cond_var;

  {
    struct spinlock *spin = (struct spinlock *)mutex->mutex_obj;
    u64 until = nsectime() + usecs * 1000000;

    scoped_acquire l(spin);
    if (until > nsectime())
      cond_var->sleep_to(spin, until);
  }
}
