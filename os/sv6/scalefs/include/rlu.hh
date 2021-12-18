#include "rlu.h"
#include "kernel.hh"
#include "cpputil.hh"

#define RLU_NEW_DELETE(CLASS_NAME)                                      \
  static void*                                                          \
  operator new(unsigned long nbytes, const std::nothrow_t&) noexcept {  \
    return rlu::rlu_alloc<CLASS_NAME>(nbytes);                          \
  }                                                                     \
  static void*                                                          \
  operator new(unsigned long nbytes) {                                  \
    void *p = CLASS_NAME::operator new(nbytes, std::nothrow);           \
    if (p == nullptr)                                                   \
      throw_bad_alloc();                                                \
    return p;                                                           \
  }                                                                     \
  static void                                                           \
  operator delete(void *p, const std::nothrow_t&) noexcept {            \
    rlu::rlu_free(p);                                                   \
  }                                                                     \
                                                                        \
  static void                                                           \
  operator delete(void *p) {                                            \
    CLASS_NAME::operator delete(p, std::nothrow);                       \
  }

namespace rlu {

  class thread_handle;

  inline void
  rlu_free(void *ptr) {
    ::rlu_free(nullptr, (intptr_t *)ptr);
  }
  
  template <typename T>
  inline T*
  rlu_alloc(obj_size_t obj_size) {
    return (T *) ::rlu_alloc(obj_size);
  }

  template <typename T>
  inline bool
  rlu_is_same_ptr(T *p_obj_1, T *p_obj_2) {
    return ::rlu_cmp_ptrs((intptr_t *)p_obj_1, (intptr_t *)p_obj_2);
  }

  template <typename T>
  inline void
  assign_pointer(T **p_ptr, T *p_obj) {
    ::rlu_assign_pointer((intptr_t **)p_ptr, (intptr_t *)p_obj);
  }

  class thread_handle {
    ::rlu_thread_data_t *self_;
  public:
    thread_handle() {
      self_ = (rlu_thread_data_t *) kmalloc(sizeof(rlu_thread_data), "rlu_data");
      ::rlu_thread_init(self_);
    }

    ~thread_handle() {
      ::rlu_thread_finish(self_);
      kmfree((void *)self_, sizeof(rlu_thread_data));
    }

    inline void
    reader_lock() {
      ::rlu_reader_lock(self_);
    }

    inline void
    reader_unlock() {
      ::rlu_reader_unlock(self_);
    }

    template <typename T>
    inline bool
    try_lock(T **p_p_obj) {
      if (!*p_p_obj)
        return true;
      return ::rlu_try_lock(self_, (intptr_t **)p_p_obj, sizeof(T));
    }

    template <typename T>
    inline bool
    try_lock_const(T *p_obj) {
      if (!p_obj)
        return true;
      return ::rlu_try_lock(self_, (intptr_t **)&p_obj, sizeof(T));
    }

    void
    abort() {
      ::rlu_abort(self_);
    }

    template <typename T>
    T *
    deref_ptr(T* p_obj) {
      intptr_t *p_cur_obj;
      if (rlu_likely(self_->is_check_locks == 0))
        p_cur_obj = (intptr_t *)p_obj;
      else
      {
        if (rlu_likely((p_obj != NULL) && RLU_IS_UNLOCKED(p_obj)))
        {
          p_cur_obj = (intptr_t *)p_obj;
        }
        else
        {
          p_cur_obj = ::rlu_deref_slow_path(self_, (intptr_t *)p_obj);
		}
      }
      return (T*)p_cur_obj;
    }

    template <typename T>
    void
    free(T *p_obj) {
      ::rlu_free(self_, (intptr_t *)p_obj);
    }
    
  };

}
