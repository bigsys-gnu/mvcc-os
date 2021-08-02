#include "types.h"
#include "pthread.h"
#include "user.h"
#include "limits.h"
#include <atomic>
#include "elfuser.hh"
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include "futex.h"
#define M_CAS(shared, old_v, new_v) __sync_val_compare_and_swap(shared, old_v, new_v)
#define M_FAS(shared, dec) __sync_fetch_and_sub(shared, dec)

enum { stack_size = 8192 };
static std::atomic<int> nextkey;
enum { max_keys = 128 };
enum { elf_tls_reserved = 1 };

struct tlsdata {
  void* tlsptr[elf_tls_reserved];
  void* buf[max_keys];
};

void
forkt_setup(u64 pid)
{
  static size_t memsz;
  static size_t filesz;
  static size_t align;
  static void* initimage;

  if (initimage == 0 && _dl_phdr) {
    for (proghdr* p = _dl_phdr; p < &_dl_phdr[_dl_phnum]; p++) {
      if (p->type == ELF_PROG_TLS) {
        memsz = p->memsz;
        filesz = p->filesz;
        initimage = (void *) p->vaddr;
        align = p->align;
        break;
      }
    }
  }

  u64 memsz_align = (memsz+align-1) & ~(align-1);

  s64 tptr = (s64) sbrk(sizeof(tlsdata) + memsz_align);
  assert(tptr != -1);

  memcpy((void*)tptr, initimage, filesz);
  tlsdata* t = (tlsdata*) (tptr + memsz_align);
  t->tlsptr[0] = t;
  setfs((u64) t);
}

int
pthread_create(pthread_t* tid, const pthread_attr_t* attr,
               void* (*start)(void*), void* arg)
{
  char* base = (char*) sbrk(stack_size);
  assert(base != (char*)-1);
  int t = forkt(base + stack_size, (void*) start, arg, FORK_SHARE_VMAP | FORK_SHARE_FD);
  if (t < 0)
    return t;

  *tid = t;
  return 0;
}

int
pthread_createflags(pthread_t* tid, const pthread_attr_t* attr,
                    void* (*start)(void*), void* arg, int flag)
{
  char* base = (char*) sbrk(stack_size);
  assert(base != (char*)-1);
  int t = forkt(base + stack_size, (void*) start, arg, FORK_SHARE_VMAP);
  if (t < 0)
    return t;

  *tid = t;
  return 0;
}

int
xthread_create(pthread_t* tid, int flags,
               void* (*start)(void*), void* arg)
{
  char* base = (char*) sbrk(stack_size);
  assert(base != (char*)-1);
  int t = forkt(base + stack_size, (void*) start, arg,
                FORK_SHARE_VMAP | FORK_SHARE_FD | flags);
  if (t < 0)
    return t;

  *tid = t;
  return 0;
}

void
pthread_exit(void* retval)
{
  exit(0);
}

int
pthread_join(pthread_t tid, void** retval)
{
  if (retval) {
    printf("XXX join retval\n");
    *retval = 0;
  }

  waitpid(tid, NULL,0);
  return 0;
}

pthread_t
pthread_self()
{
  return getpid();
}

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
  // Ignore the destructor for now.
  int k = nextkey++;
  if (k >= max_keys)
    return -1;

  *key = k + elf_tls_reserved;   // skip a few slots for ELF-TLS
  return 0;
}

void*
pthread_getspecific(pthread_key_t key)
{
  u64 v;
  __asm volatile("movq %%fs:(%1), %0" : "=r" (v) : "r" ((u64) key * 8));
  return (void*) v;
}

int
pthread_setspecific(pthread_key_t key, void* value)
{
  __asm volatile("movq %0, %%fs:(%1)" : : "r" (value), "r" ((u64) key * 8) : "memory");
  return 0;
}

int
pthread_barrier_init(pthread_barrier_t *b,
                     const pthread_barrierattr_t *attr, unsigned count)
{
  b->store(count);
  return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *b)
{
  (*b)--;
  while (*b != 0)
    ;   // spin
  return 0;
}

int
sched_setaffinity(int pid, size_t cpusetsize, cpu_set_t *mask)
{
  assert(!mask->empty_flag);
  return setaffinity(mask->the_cpu);
}

int       
pthread_spin_init(pthread_spinlock_t *spin, const pthread_spinlockattr_t *attr)
{
  *spin = 0;
  return 0;
}

int 
pthread_spin_lock(pthread_spinlock_t *spin)
{
  while(!__sync_bool_compare_and_swap(spin, 0, 1)) ;
  return 0;
}

int
pthread_spin_trylock(pthread_spinlock_t *spin)
{
  return __sync_bool_compare_and_swap(spin, 0, 1);
}

int 
pthread_spin_unlock(pthread_spinlock_t *spin)
{
  int b = __sync_bool_compare_and_swap(spin, 1, 0);
  return !b;
}

// Mutex with futex example from
// https://eli.thegreenplace.net/2018/basics-of-futexes/

int
pthread_mutex_init(pthread_mutex_t *mutex,
                   const pthread_mutexattr_t *attr)
{
  *mutex = 0;
  return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  *mutex = 0;
  __sync_synchronize();
  futex((const u64 *) mutex, FUTEX_WAKE, INT_MAX, 0);
  return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
  int c = M_CAS(mutex, 0, 1);
  // If the lock was previously unlocked, there's nothing else for us to do.
  // Otherwise, we'll probably have to wait.
  if (c != 0)
  {
    do {
      // If the mutex is locked, we signal that we're waiting by setting the
      // atom to 2. A shortcut checks is it's 2 already and avoids the atomic
      // operation in this case.
      if (c == 2 || M_CAS(mutex, 1, 2) != 0)
      {
        // Here we have to actually sleep, because the mutex is actually
        // locked. Note that it's not necessary to loop around this syscall;
        // a spurious wakeup will do no harm since we only exit the do...while
        // loop when atom_ is indeed 0.
        futex((const u64 *)mutex, FUTEX_WAIT, 2, 0);
      }
      // We're here when either:
      // (a) the mutex was in fact unlocked (by an intervening thread).
      // (b) we slept waiting for the atom and were awoken.
      //
      // So we try to lock the atom again. We set teh state to 2 because we
      // can't be certain there's no other thread at this exact point. So we
      // prefer to err on the safe side.
    } while((c = M_CAS(mutex, 0, 2) != 0));
  }
  return 1;
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
  return M_CAS(mutex, 0, 1) == 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  if (M_FAS(mutex, 1) != 1)
  {
    *mutex = 0;
    __sync_synchronize();
    futex((const u64 *) mutex, FUTEX_WAKE, 1, 0);
  }
  return 1;
}


// simple condition implementation with futex
// from https://www.remlab.net/op/futex-condvar.shtml
// Copyrigt 2004-2021 Remi Denis-Courmont

int
pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t * mock)
{
  *cv = (pthread_cond_t) {0};
  return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cv)
{
  // do nothing
  (void)cv;
  return 0;
}

int
pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *mtx)
{
  return pthread_cond_timedwait(cv, mtx, NULL);
}

int
pthread_cond_signal(pthread_cond_t *cv)
{
  unsigned value = 1u + __atomic_load_n(&cv->previous, __ATOMIC_SEQ_CST);
  __atomic_store_n(&cv->value, value, __ATOMIC_SEQ_CST);

  futex((const u64 *)&cv->value, FUTEX_WAKE, 1, 0);

  return 1;
}

int
pthread_cond_timedwait(pthread_cond_t *cv, pthread_mutex_t *mtx,
                       const struct timespec *ts)
{
  int value = __atomic_load_n(&cv->value, __ATOMIC_SEQ_CST);
  __atomic_store_n(&cv->previous, value, __ATOMIC_SEQ_CST);

  pthread_mutex_unlock(mtx);
  futex((const u64 *)&cv->value, FUTEX_WAIT, value,
        (u64)(ts ? ts->after_nano_sec : 0));
  pthread_mutex_lock(mtx);
  return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *cv)
{
  unsigned value = 1u + __atomic_load_n(&cv->previous, __ATOMIC_SEQ_CST);

  __atomic_store_n(&cv->value, value, __ATOMIC_SEQ_CST);

  futex((const u64 *)&cv->value, FUTEX_WAKE, INT_MAX, 0);

  return 1;
}
