#pragma once

#include "mvrlu/mvrlu.hh"
#include "kernel.hh"
#include "cpu.hh"
#include "proc.hh"

namespace mvrlu {

  template <typename T> struct link;
  template <typename T, link<T> T::* L> class list;
  template <typename T, link<T> T::* L> class iter;

  template <typename T>
  struct link {
    T* next;

    constexpr link(void): next(nullptr) {}
    constexpr link(T* o): next(o) {}

    link
    operator=(link& rhs)
    {
      mvrlu_assign_pointer(&next, rhs.next);
      return link(next);
    }

    link
    operator=(T *rhs)
    {
      mvrlu_assign_pointer(&next, rhs);
      return link(next);
    }

    T*
    operator->(void)
    {
      return next;
    }

    T&
    operator*(void)
    {
      return *next;
    }
  };

  template <typename T, link<T> T::* L>
  class iter {
    friend class list<T, L>;
    T *ptr_;
  public:
    constexpr iter(void): ptr_(nullptr) {}
    iter(const iter &o)
    {
      auto &h = *myproc()->handle;
      ptr_ = h.mvrlu_deref(o.ptr_);
    }
    iter(T *ptr) {
      auto &h = *myproc()->handle;
      ptr_ = h.mvrlu_deref(ptr);
    }

    bool
    try_lock(void)
    {
      auto &h = *myproc()->handle;
      bool ret = h.mvrlu_try_lock(&ptr_);
      if (!ret)
        h.mvrlu_abort();
      return ret;
    }

    bool
    try_lock_const(void)
    {
      auto &h = *myproc()->handle;
      bool ret = h.mvrlu_try_lock_const(ptr_);
      if (!ret)
        h.mvrlu_abort();
      return ret;
    }

    T &
    operator*(void) const
    {
      return *ptr_;
    }

    T *
    operator->(void) const
    {
      return ptr_;
    }

    iter
    operator=(const iter& rhs)
    {
      ptr_ = rhs.ptr_;
      return *this;
    }

    iter
    operator=(T *rhs)
    {
      auto &h = *myproc()->handle;
      ptr_ = h.mvrlu_deref(rhs);

      return *this;
    }

    iter &
    operator++(void)
    {
      auto &h = *myproc()->handle;
      ptr_ = h.mvrlu_deref((ptr_->*L).next);

      return *this;
    }

    iter
    operator++(int)
    {
      iter cur = *this;
      ++(*this);
      return cur;
    }

    bool
    operator==(T *rhs) const noexcept
    {
      return rhs ?
        mvrlu_cmp_ptrs(ptr_, rhs) : (ptr_ == rhs);
    }

    bool
    operator==(iter &rhs) const noexcept
    {
      return rhs.ptr_ ?
        mvrlu_cmp_ptrs(ptr_, rhs.ptr_) : (ptr_ == rhs.ptr_);
    }

    bool
    operator!=(T *rhs) const noexcept {
      return !(*this == rhs);
    }

    bool
    operator!=(iter &rhs) const noexcept {
      return !(*this == rhs);
    }

    operator T * ()
    {
      return ptr_;
    }
  };

  template <typename T, link<T> T::* L>
  class list {
    link<T> head_;

  public:
    typedef iter<T, L> iterator;

    list(void) {
      // this can cause a lot of memory waste
      head_.next = mvrlu_alloc<T>();
      (head_.next->*L).next = nullptr;
    }

    list(const list &o) = delete;
    list & operator=(const list &o) = delete;
    list(list &&o) = delete;

    iterator
    before_begin() noexcept
    {
      return iterator(head_.next);
    }

    iterator
    begin(void) noexcept
    {
      return iterator((head_.next->*L).next);
    }

    const iterator
    begin() const noexcept
    {
      return iterator((head_.next->*L).next);
    }

    iterator
    end() noexcept
    {
      return iterator();
    }

    const iterator
    end() const noexcept
    {
      return iterator();
    }

    bool
    empty() const noexcept
    {
      return iterator(head_) == iterator();
    }

    void
    insert_after(iterator pos, T* x) noexcept
    {
      (x->*L) = (pos.ptr_->*L);
      (pos.ptr_->*L) = x;
    }

    void
    erase_after(iterator pos) noexcept
    {
      (pos.ptr_->*L) = ((pos.ptr_->*L).next->*L);
    }

    iterator
    erase_after(iterator pos, iterator last) noexcept
    {
      (pos.ptr_->*L) = (last.ptr_->*L);
      return last;
    }

    /*
     * Return an iterator pointing to elem, which must be in this list.
     */
    iterator
    iterator_to(T *elem)
    {
      return iterator(elem);
    }

  };

}
