#pragma once

#include "mvrlu.h"
#include "mvrlu_i.h"
#include "cpputil.hh"
#include "mvrlu/port-kernel.h"
#include <cstddef>

namespace mvrlu {
  class thread_handle;

  inline void
  mvrlu_free(void *ptr) {
    ::mvrlu_free(NULL, ptr);
  }

  // call RLU_INIT, RLU_FINISH manually
  // this mvrlu can be applied only one data structure.
  template <typename T>
  inline void
  mvrlu_assign_pointer(T **p_ptr, T *obj) {
    ::_mvrlu_assign_pointer((void **)p_ptr, (void *)obj);
  }

  template <typename T>
  inline bool
  mvrlu_cmp_ptrs(T *obj, T *obj2) {
    return ::mvrlu_cmp_ptrs((void *)obj, (void *)obj2);
  }

  template <typename T>
  inline void *
  mvrlu_alloc(void) {
    return (T *) ::mvrlu_alloc(sizeof(T));
  }

  class thread_handle {
  public:
    thread_handle(void) {
      ::mvrlu_thread_init(&self_);
    }

    ~thread_handle(void) {
      ::mvrlu_thread_finish(&self_);
      ::mvrlu_thread_free(&self_);
    }

    // don't deallocate memory right away
    // _qp_thread will take care of.
    static void operator delete(void *, std::size_t) {}

    static void* operator new(unsigned long nbytes, const std::nothrow_t&) noexcept {
      assert(nbytes == sizeof(thread_handle));
      return port_alloc_x(sizeof(thread_handle), 0);
    }

    static void* operator new(unsigned long nbytes) {
      void *p = thread_handle::operator new(nbytes, std::nothrow);
      if (p == nullptr)
        throw_bad_alloc();
      return p;
    }

    inline void
    mvrlu_reader_lock(void) {
      ::mvrlu_reader_lock(&self_);
    }

    inline void
    mvrlu_reader_unlock(void) {
      ::mvrlu_reader_unlock(&self_);
    }

    template <typename T>
    inline bool
    mvrlu_try_lock(T** p_p_obj) {
      if (!*p_p_obj)
        return true;
      return ::_mvrlu_try_lock(&self_, (void **)p_p_obj, sizeof(T));
    }

    template <typename T>
    inline bool
    mvrlu_try_lock_const(T* obj) {
      if (!obj)
        return true;
      return ::_mvrlu_try_lock_const(&self_, (void *)obj, sizeof(T));
    }

    inline void
    mvrlu_abort(void) {
      ::mvrlu_abort(&self_);
    }

    template <typename T>
    inline T*
    mvrlu_deref(T *p_obj) {
      return (T *) ::mvrlu_deref(&self_, (void *)p_obj);
    }

    // need hotfix!!!
    // - how to call deleter of p_obj properly
    // 1. mvrlu.c don't know p_obj is which type.
    // -> mvrlu.c need to be fixed using template?
    template <typename T>
    inline void
    mvrlu_free(T *p_obj) {
      ::mvrlu_free(&self_, (void *)p_obj);
    }

    void
    mvrlu_flush_log(void) {
      ::mvrlu_flush_log(&self_);
    }

  private:
    mvrlu_thread_struct_t self_;
  };

}
