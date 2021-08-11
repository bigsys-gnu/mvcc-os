#ifndef BIND_C_H
#define BIND_C_H

#include "port_data.h"

/*
 *  logs
 */

int __port_log_region_init(unsigned long size, unsigned long num);
void __port_log_region_destroy(void);
void *__port_alloc_log_mem(void);
void __port_free_log_mem(void *addr);
int __port_addr_in_log_region(void *addr__);

/*
 * Memory allocation
 */

void *__port_alloc_x(size_t size, unsigned int flags);
void __port_free(void *ptr);

/*
 * Synchronization
 */

void __port_cpu_relax_and_yield(void);
void __port_spin_init(spinlock_t *lock);
void __port_spin_destroy(spinlock_t *lock);
void __port_spin_lock(spinlock_t *lock);
int __port_spin_trylock(spinlock_t *lock);
void __port_spin_unlock(spinlock_t *lock);
int __port_mutex_init(struct mutex *mutex);
int __port_mutex_destroy(struct mutex *mutex);
int __port_mutex_lock(struct mutex *mutex);
int __port_mutex_unlock(struct mutex *mutex);
void __port_cond_init(struct completion *cond);
void __port_cond_destroy(struct completion *cond);

/*
 * Thread
 */

int __port_create_thread(const char *name, struct task_struct **t,
                       int (*fn)(void *), void *arg, struct completion *completion);

void __port_finish_thread(struct completion *completion);

void __port_wait_for_finish(void *x, struct completion *completion);

void __port_initiate_wakeup(struct mutex *mutex,
                                 struct completion *cond);

void __port_initiate_nap(struct mutex *mutex, struct completion *cond,
                              unsigned long usecs);


#endif /* BIND_C_H */
