#include "mvrlu/port-kernel.h"
#include "mvrlu/bind_c.h"

inline int port_log_region_init(unsigned long size, unsigned long num)
{
  return __port_log_region_init(size, num);
}

inline void port_log_region_destroy(void)
{
  __port_log_region_destroy();
}

inline void *port_alloc_log_mem(void)
{
  return __port_alloc_log_mem();
}

inline void port_free_log_mem(void *addr)
{
  __port_free_log_mem(addr);
}

inline int port_addr_in_log_region(void *addr__)
{
  return __port_addr_in_log_region(addr__);
}

/*
 * Memory allocation
 */

inline void *port_alloc_x(size_t size, unsigned int flags)
{
  return __port_alloc_x(size, flags);
}

inline void port_free(void *ptr)
{
  __port_free(ptr);
}

/*
 * Synchronization
 */

inline void port_cpu_relax_and_yield(void)
{
  __port_cpu_relax_and_yield();
}

inline void port_spin_init(spinlock_t *lock)
{
  __port_spin_init(lock);
}

inline void port_spin_destroy(spinlock_t *lock)
{
  __port_spin_destroy(lock);
}

inline void port_spin_lock(spinlock_t *lock)
{
  __port_spin_lock(lock);
}

inline int port_spin_trylock(spinlock_t *lock)
{
  return __port_spin_trylock(lock);
}

inline void port_spin_unlock(spinlock_t *lock)
{
  __port_spin_unlock(lock);
}

inline int port_mutex_init(struct mutex *mutex)
{
  return __port_mutex_init(mutex);
}

inline int port_mutex_destroy(struct mutex *mutex)
{
  return __port_mutex_destroy(mutex);
}

inline int port_mutex_lock(struct mutex *mutex)
{
  return __port_mutex_lock(mutex);
}

inline int port_mutex_unlock(struct mutex *mutex)
{
  return __port_mutex_unlock(mutex);
}

inline void port_cond_init(struct completion *cond)
{
  __port_cond_init(cond);
}

inline void port_cond_destroy(struct completion *cond)
{
  __port_cond_destroy(cond);
}

/*
 * Thread
 */

inline int port_create_thread(const char *name, struct task_struct **t,
                              void (*fn)(void *), void *arg,
                              struct completion *completion)
{
  return __port_create_thread(name, t, fn, arg, completion);
}

inline void port_finish_thread(struct completion *completion)
{
  __port_finish_thread(completion);
}

inline void port_wait_for_finish(void *x, struct completion *completion)
{
  __port_wait_for_finish(x, completion);
}

inline void port_initiate_wakeup(struct mutex *mutex,
                                 struct completion *cond)
{
  __port_initiate_wakeup(mutex, cond);
}

inline void port_initiate_nap(struct mutex *mutex, struct completion *cond,
                              unsigned long usecs)
{
  __port_initiate_nap(mutex, cond, usecs);
}
