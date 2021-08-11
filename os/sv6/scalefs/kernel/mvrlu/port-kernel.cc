#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "vmalloc.hh"
#include "proc.hh"
#include "mvrlu/arch.h"
extern "C" {
#include "mvrlu/bind_c.h"
}

void test_mvrlu(void) {
  cprintf("mvrlu makefile\n");
}

/*
 * Log region
 */
static unsigned long g_size __read_mostly;

inline int __port_log_region_init(unsigned long size, unsigned long num)
{
  g_size = size;
  return 0;
}

inline void __port_log_region_destroy(void)
{
	/* do nothing */
}

inline void *__port_alloc_log_mem(void)
{
  return vmalloc_raw(g_size, 4, "port log");
}

inline void __port_free_log_mem(void *addr)
{
  vmalloc_free(addr);
}

inline int __port_addr_in_log_region(void *addr__)
{
	unsigned long addr = (unsigned long)addr__;
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

inline void *__port_alloc_x(size_t size, unsigned int flags)
{
  struct alloc_mem *mem;

  kmalign(&mem, 4, size + sizeof(size_t), "port mem");
  mem->size = size + sizeof(size_t);
  return mem->byte;
}

inline void __port_free(void *ptr)
{
  struct alloc_mem *mem = (struct alloc_mem *)(ptr - sizeof(void *));
  kmalignfree(mem, 4, mem->size);
}

inline void __port_cpu_relax_and_yield(void)
{
  yield();
  cpu_relax();
}

inline void __port_spin_init(spinlock_t *lock)
{
  lock->spin_obj = (void *) new spinlock("port lock");
}

inline void __port_spin_destroy(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;

  if (l)
    delete l;
}

inline void __port_spin_lock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  l->acquire();
}

inline int __port_spin_trylock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->try_acquire();
}

inline void __port_spin_unlock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->release();
}

inline int __port_mutex_init(struct mutex *mutex)
{
  mutex->mutex_obj = (void *) new spinlock("port mutex");
}

inline int __port_mutex_destroy(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;

  if (l)
    delete l;
}

inline int __port_mutex_lock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->acquire();
}

inline int __port_mutex_unlock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->release();
}

inline void __port_cond_init(struct completion *cond)
{
  cond->cond_obj = (void *) new condvar("port cond")
}

inline void __port_cond_destroy(struct completion *cond)
{
  condvar *cond = (condvar *)cond->cond_obj;
  delete cond;
}

/*
 * Thread
 */

int __port_create_thread(const char *name, struct task_struct **t,
                              int (*fn)(void *), void *arg,
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
    *t = temp;
    return 0;
  }
  return -11
}

void __port_finish_thread(struct completion *completion)
{
  struct condvar *cond = (struct condvar *) completion->cond_obj;
  cond->wake_all();
}

void __port_wait_for_finish(void *x, struct completion *completion)
{
  struct condvar *cond = (struct condvar *) completion->cond_obj;
  struct spinlock local_lock("local");

  scoped_acquire lg(&local_lock);
  cond->sleep(&local_lock);
}

inline void __port_initiate_wakeup(struct mutex *mutex, struct completion *cond)
{
  struct condvar *cond_var = (struct condvar *) cond->cond_obj;
  (void)mutex;
  cond_var->wake_all();
}

inline void __port_initiate_nap(struct mutex *mutex, struct completion *cond,
                              unsigned long usecs)
{
  struct condvar *cond_var = (struct condvar *) cond->cond_obj;

  delete cond_var;
  cond_var = new condvar("port cond");
  cond->cond_obj = (void *) cond_var;

  {
    struct spinlock *mutex = (struct spinlock *)mutex->mutex_obj;
    u64 until = nsectime() + usecs * 1000000;

    scoped_acquire l(mutex);
    if (until > nsectime())
      cond.sleep_to(mutex, until);

    stop = 1;
  }
}
