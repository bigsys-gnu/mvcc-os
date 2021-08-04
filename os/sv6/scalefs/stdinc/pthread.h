#pragma once

/*
 * Our minimal version of pthreads
 */

#ifdef __cplusplus
#include <atomic>
#endif

typedef int pthread_t;
typedef int pthread_attr_t;
typedef int pthread_key_t;
typedef int pthread_barrierattr_t;
typedef int pthread_spinlock_t;
typedef int pthread_spinlockattr_t;
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
typedef struct {
  unsigned int value;
  unsigned int previous;
} pthread_cond_t;
typedef int pthread_condattr_t;
struct timespec {
  unsigned long after_nano_sec;
};

#ifdef __cplusplus
typedef std::atomic<unsigned> pthread_barrier_t;
#else
typedef unsigned pthread_barrier_t;
#endif

BEGIN_DECLS

int       pthread_create(pthread_t* tid, const pthread_attr_t* attr,
                         void* (*start)(void*), void* arg);
int       pthread_createflags(pthread_t* tid, const pthread_attr_t* attr,
                         void* (*start)(void*), void* arg, int fdshare);
pthread_t pthread_self(void);

int       pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
void*     pthread_getspecific(pthread_key_t key);
int       pthread_setspecific(pthread_key_t key, void* value);

int       pthread_barrier_init(pthread_barrier_t *b,
                               const pthread_barrierattr_t *attr,
                               unsigned count);
int       pthread_barrier_wait(pthread_barrier_t *b);

int       pthread_spin_init(pthread_spinlock_t *, pthread_spinlockattr_t);
int       pthread_spin_destroy(pthread_spinlock_t *);
int       pthread_spin_lock(pthread_spinlock_t *);
int       pthread_spin_trylock(pthread_spinlock_t *);
int       pthread_spin_unlock(pthread_spinlock_t *);

int       pthread_mutex_init(pthread_mutex_t *mutex,
                             const pthread_mutexattr_t *attr);
int       pthread_mutex_destroy(pthread_mutex_t *mutex);
int       pthread_mutex_lock(pthread_mutex_t *mutex);
int       pthread_mutex_trylock(pthread_mutex_t *mutex);
int       pthread_mutex_unlock(pthread_mutex_t *mutex);

int       pthread_join(pthread_t tid, void **retvalp);
void      pthread_exit(void *retval) __noret__;

int       pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
int       pthread_cond_destroy(pthread_cond_t *);
int       pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
int       pthread_cond_timedwait(pthread_cond_t *, pthread_mutex_t *, const struct timespec *);
int       pthread_cond_broadcast(pthread_cond_t *);
int       pthread_cond_signal(pthread_cond_t *);

// Special xv6 pthread_create, flags is FORK_* bits
int       xthread_create(pthread_t* tid, int flags,
                         void* (*start)(void*), void* arg);

END_DECLS
