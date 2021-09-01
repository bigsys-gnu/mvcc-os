#ifndef PORT_KERNEL_H
#define PORT_KERNEL_H

#include "port_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;

/*
 *  logs
 */

int port_log_region_init(unsigned long size, unsigned long num);
void port_log_region_destroy(void);
void *port_alloc_log_mem(void);
void port_free_log_mem(void *addr);
int port_addr_in_log_region(void *addr__);

/*
 * Memory allocation
 */

void *port_alloc_x(size_t size, unsigned int flags);
void port_free(void *ptr);

/*
 * Synchronization
 */

void port_cpu_relax_and_yield(void);
void port_spin_init(spinlock_t *lock);
void port_spin_destroy(spinlock_t *lock);
void port_spin_lock(spinlock_t *lock);
int port_spin_trylock(spinlock_t *lock);
void port_spin_unlock(spinlock_t *lock);
int port_mutex_init(struct mutex *mutex);
int port_mutex_destroy(struct mutex *mutex);
int port_mutex_lock(struct mutex *mutex);
int port_mutex_unlock(struct mutex *mutex);
void port_cond_init(struct completion *cond);
void port_cond_destroy(struct completion *cond);

/*
 * Thread
 */

int port_create_thread(const char *name, struct task_struct **t,
                       void (*fn)(void *), void *arg, struct completion *completion);

void port_finish_thread(struct completion *completion);

void port_wait_for_finish(void *x, struct completion *completion);

void port_initiate_wakeup(struct mutex *mutex,
                                 struct completion *cond);

void port_initiate_nap(struct mutex *mutex, struct completion *cond,
                              unsigned long usecs);

void port_print_str(const char *str, ...);

#ifdef __cplusplus
}
#endif

#endif /* PORT-KERNEL_H */
