#ifndef BIND_C_H
#define BIND_C_H

/*
 * Data Types
 */

typedef struct {
  void *spin_obj;
} spinlock_t;

struct mutex {
  void *mutex_obj;
};

struct completion {
  void *cond_obj;
};

/*
 *  logs
 */

inline int __port_log_region_init(unsigned long size, unsigned long num);
inline void __port_log_region_destroy(void);
inline void *__port_alloc_log_mem(void);
inline void __port_free_log_mem(void *addr);
inline int __port_addr_in_log_region(void *addr__);

/*
 * Memory allocation
 */

inline void *__port_alloc_x(size_t size, unsigned int flags);
inline void __port_free(void *ptr);

/*
 * Synchronization
 */

inline void __port_cpu_relax_and_yield(void);
inline void __port_spin_init(spinlock_t *lock);
inline void __port_spin_destroy(spinlock_t *lock);
inline void __port_spin_lock(spinlock_t *lock);
inline int __port_spin_trylock(spinlock_t *lock);
inline void __port_spin_unlock(spinlock_t *lock);
inline int __port_mutex_init(struct mutex *mutex);
inline int __port_mutex_destroy(struct mutex *mutex);
inline int __port_mutex_lock(struct mutex *mutex);
inline int __port_mutex_unlock(struct mutex *mutex);
inline void __port_cond_init(struct completion *cond);
inline void __port_cond_destroy(struct completion *cond);

/*
 * Thread
 */

int __port_create_thread(const char *name, struct task_struct **t,
                       int (*fn)(void *), void *arg, struct completion *completion);

void __port_finish_thread(struct completion *completion);

void __port_wait_for_finish(void *x, struct completion *completion);

inline void __port_initiate_wakeup(struct mutex *mutex,
                                 struct completion *cond);

inline void __port_initiate_nap(struct mutex *mutex, struct completion *cond,
                              unsigned long usecs);


#endif /* BIND_C_H */
