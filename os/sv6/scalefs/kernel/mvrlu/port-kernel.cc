#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "vmalloc.hh"
extern "C" {
#include "mvrlu/port-kernel.h"
}

void test_mvrlu(void) {
  cprintf("mvrlu makefile\n");
}

/*
 * Log region
 */
static unsigned long g_size __read_mostly;

inline int port_log_region_init(unsigned long size, unsigned long num)
{
  g_size = size;
  return 0;
}

inline void port_log_region_destroy(void)
{
	/* do nothing */
}

inline void *port_alloc_log_mem(void)
{
  return vmalloc_raw(g_size, 4, "port log");
}

inline void port_free_log_mem(void *addr)
{
  vmalloc_free(addr);
}

inline int port_addr_in_log_region(void *addr__)
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

inline void *port_alloc_x(size_t size, unsigned int flags)
{
  struct alloc_mem *mem;

  kmalign(&mem, 4, size + sizeof(size_t), "port mem");
  mem->size = size + sizeof(size_t);
  return mem->byte;
}

inline void port_free(void *ptr)
{
  struct alloc_mem *mem = (struct alloc_mem *)(ptr - sizeof(void *));
  kmalignfree(mem, 4, mem->size);
}

inline void port_spin_init(spinlock_t *lock)
{
  lock->spin_obj = (void *) new spinlock("port lock");
}

inline void port_spin_destroy(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;

  if (l)
    delete l;
}

inline void port_spin_lock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  l->acquire();
}

inline int port_spin_trylock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->try_acquire();
}

inline void port_spin_unlock(spinlock_t *lock)
{
  spinlock *l = (spinlock *)lock->spin_obj;
  return l->release();
}

inline int port_mutex_init(struct mutex *mutex)
{
  mutex->mutex_obj = (void *) new spinlock("port mutex");
}

inline int port_mutex_destroy(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;

  if (l)
    delete l;
}

inline int port_mutex_lock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->acquire();
}

inline int port_mutex_unlock(struct mutex *mutex)
{
  spinlock *l = (spinlock *)mutex->mutex_obj;
  l->release();
}

inline void port_cond_init(struct completion *cond)
{
  cond->cond_obj = (void *) new condvar("port cond")
}

inline void port_cond_destroy(struct completion *cond)
{
  condvar *cond = (condvar *)cond->cond_obj;
}
