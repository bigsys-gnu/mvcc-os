#include <new>

#include "types.h"
#include "kernel.hh"
#include "cpputil.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "elf.hh"
#include "atomic_util.hh"
#include "errno.h"

// const std::nothrow_t std::nothrow;

void* stdout = nullptr;
void* stderr = nullptr;

void*
operator new(std::size_t nbytes)
{
  panic("global operator new");

  u64* x = (u64*) kmalloc(nbytes + sizeof(u64), "cpprt new");
  *x = nbytes;
  return x+1;
}

void*
operator new(std::size_t nbytes, std::align_val_t al)
{
  size_t alignment = (size_t)al;
  if(alignment < sizeof(u64))
    alignment = sizeof(u64);

  char* x = (char*)kmalloc(nbytes + alignment, "cpprt aligned new");
  *(u64*)x = nbytes;
  return x + alignment;
}

void
operator delete(void* p) noexcept
{
  panic("global operator delete");

  u64* x = (u64*) p;
  kmfree(x-1, x[-1] + sizeof(u64));
}

void*
operator new[](std::size_t nbytes)
{
  // XXX This allocation is only aligned to 8 bytes, but could be backing a
  // struct with larger allocation requirements. Anything containing a u128 or
  // long double will probably crash as a result.
  u64* x = (u64*) kmalloc(nbytes + sizeof(u64), "array");
  *x = nbytes;
  return x+1;
}

void
operator delete[](void* p) noexcept
{
  u64* x = (u64*) p;
  kmfree(x-1, x[-1] + sizeof(u64));
}

// void *
// operator new(std::size_t nbytes, void* buf) noexcept
// {
//   return buf;
// }

// void
// operator delete(void* ptr, void*) noexcept
// {
// }

// void*
// operator new[](std::size_t size, void* ptr) noexcept
// {
//   return ptr;
// }

// void
// operator delete[](void* ptr, void*) noexcept
// {
// }

void
__cxa_pure_virtual(void)
{
  panic("__cxa_pure_virtual");
}

int
__cxa_guard_acquire(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  pushcli();
  while (xchg32(l, 1) != 0)
    ; /* spin */

  if (*x) {
    xchg32(l, 0);
    popcli();
    return 0;
  }
  return 1;
}

void
__cxa_guard_release(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  *x = 1;
  __sync_synchronize();
  xchg32(l, 0);
  popcli();
}

void
__cxa_guard_abort(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  xchg32(l, 0);
  popcli();
}

int
__cxa_atexit(void (*f)(void*), void *p, void *d)
{
  return 0;
}

extern "C" void abort(void)
{
  panic("abort");
}

static void
cxx_terminate(void)
{
  static std::atomic_flag recursive = ATOMIC_FLAG_INIT;

  // In GCC, we can actually rethrow and catch the exception that led
  // to the terminate.  However, terminate may be called for other
  // reasons, such as a "throw" without an active exception, so if we
  // don't have an active exception, this will call us recursively.
  try {
    if (!recursive.test_and_set())
      throw;
  } catch (const std::exception &e) {
    panic("unhandled exception: %s", e.what());
  } catch (...) {
    panic("unhandled exception");
  }
  panic("cxx terminate");
}

static void
cxx_unexpected(void)
{
  panic("cxx unexpected");
}

void *__dso_handle;

namespace std {

  void __terminate(void (*)()) { panic("__terminate"); }
  void __unexpected(void (*)()) { panic("__unexpected"); }

  // std::ostream cout;

  // template<>
  // u128
  // atomic<u128>::load(memory_order __m) const noexcept
  // {
  //   __sync_synchronize();
  //   u128 v = _M_i;
  //   __sync_synchronize();

  //   return v;
  // }

#if 0
  // XXX(sbw) If you enable this code, you might need to
  // compile with -mcx16
  template<>
  bool
  atomic<u128>::compare_exchange_weak(u128 &__i1, u128 i2, memory_order __m)
  {
    // XXX no __sync_val_compare_and_swap for u128
    u128 o = __i1;
    bool ok = __sync_bool_compare_and_swap(&_M_i, o, i2);
    if (!ok)
      __i1 = _M_i;
    return ok;
  }
#endif
};

namespace __cxxabiv1 {
  void (*__terminate_handler)() = cxx_terminate;
  void (*__unexpected_handler)() = cxx_unexpected;
};

static bool malloc_proc = false;

// extern "C" void* malloc(size_t);
void*
malloc(size_t n)
{
  void* ptr;
  assert(!posix_memalign(&ptr, n, 8));
  return ptr;
}

// extern "C" void free(void*);
void
free(void* vp)
{
  u32 total_size = ((u32*)vp)[-1];
  u32 ptrdiff = ((u32*)vp)[-2];

  kmfree((char*)vp - ptrdiff, total_size);
}

//extern "C" void* realloc(void*, size_t);
void*
realloc(void* vp, size_t newsize) {
  free(vp);
  return malloc(newsize);
}

extern "C" int dl_iterate_phdr(void);
int
dl_iterate_phdr(void)
{
  return -1;
}

extern "C" void __stack_chk_fail(void);
void
__stack_chk_fail(void)
{
  panic("stack_chk_fail");
}

extern "C" int pthread_once(int *oncectl, void (*f)(void));
int
pthread_once(int *oncectl, void (*f)(void))
{
  if (__sync_bool_compare_and_swap(oncectl, 0, 1))
    (*f)();

  return 0;
}

extern "C" int pthread_cancel(int tid);
int
pthread_cancel(int tid)
{
  /*
   * This function's job is to make __gthread_active_p
   * in gcc/gthr-posix95.h return 1.
   */
  return 0;
}

extern "C" int pthread_mutex_lock(int *mu);
int
pthread_mutex_lock(int *mu)
{
  while (!__sync_bool_compare_and_swap(mu, 0, 1))
    ; /* spin */
  return 0;
}

extern "C" int pthread_mutex_unlock(int *mu);
int
pthread_mutex_unlock(int *mu)
{
  *mu = 0;
  return 0;
}

extern "C" void* __cxa_get_globals(void);
void*
__cxa_get_globals(void)
{
  return myproc()->__cxa_eh_global;
}

extern "C" void* __cxa_get_globals_fast(void);
void*
__cxa_get_globals_fast(void)
{
  return myproc()->__cxa_eh_global;
}

extern "C" void __register_frame(u8*);
void
initcpprt(void)
{
#if EXCEPTIONS
  extern u8 __eh_frame_start[];
  __register_frame(__eh_frame_start);

  // Initialize lazy exception handling data structures
  try {
    throw 5;
  } catch (int& x) {
    assert(x == 5);
    malloc_proc = true;
    return;
  }

  panic("no catch");
#endif
}

extern "C" void sprintf(char *buf, const char *fmt, ...);
void sprintf(char *buf, const char *fmt, ...) {
  panic("sprintf");
}

extern "C" void __sprintf_chk(char *buf, const char *fmt, ...);
void __sprintf_chk(char *buf, const char *fmt, ...) {
  panic("sprintf");
}

extern "C" int fputs(const char* str, void*stream);
int fputs(const char* str, void*stream) {
  panic("fputs");
}

extern "C" int fputc(char c, void *stream);
int fputc(char c, void *stream) {
  panic("fputc");
}

extern "C" size_t fwrite(const void *buffer, size_t size, size_t count, void* stream);
size_t fwrite(const void *buffer, size_t size, size_t count, void* stream) {
  panic("fwrite");
}

extern "C" const unsigned short ** __ctype_b_loc() { panic("__ctype_b_loc"); }
extern "C" int32_t** __ctype_tolower_loc() { panic("__ctype_tolower_loc"); }

extern "C" int fprintf(void*, const char* format, ...) { panic("fprintf"); }
extern "C" int vfprintf(void*, const char* format, va_list vlist) { panic("vfprintf"); }

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (alignment < sizeof(void*) || (alignment & (alignment-1)))
    return EINVAL;

  if (size >= 2<<30)
    return ENOMEM;

  void* ptr = kmalloc(alignment+size, "posix_memalign");
  *memptr = (void*)(((((u64)ptr) + 8) | (alignment-1)) + 1);
  ((u32*)*memptr)[-1] = alignment+size;
  ((u32*)*memptr)[-2] = (char*)*memptr - (char*)ptr;

  return 0;
}
extern "C" void* calloc( size_t num, size_t size ) { panic("calloc"); }

extern "C" void __assert_fail(const char *assertion, const char *file, int line,
                              const char *function) {
  panic("Assertion failed: %s, function %s, file %s, line %d",
        assertion, function, file, line);
}
// std::logic_error::logic_error(char const* s): __imp_(s) {}
// std::runtime_error::runtime_error(char const* s): __imp_(s) {}
