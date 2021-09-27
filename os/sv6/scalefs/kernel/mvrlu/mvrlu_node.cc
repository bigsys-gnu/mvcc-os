#include "types.h"
#include "mvrlu/mvrlu.hpp"
#include "bench/data.hpp"

static void* mvrlu_node::operator new(unsigned long nbytes,
                                      const std::nothrow_t&) noexcept {
  return mvrlu::mvrlu_alloc<mvrlu_node>();
}

static void* mvrlu_node::operator new(unsigned long nbytes) {
  void *p = mvrlu_node::operator new(nbytes, std::nothrow);
  if (p == nullptr)
    throw_bad_alloc();
  return p;
}

static void mvrlu_node::operator delete(void *p, const std::nothrow_t&) noexcept {
  mvrlu::mvrlu_free(p);
}

static void mvrlu_node::operator delete(void *p) {
  mvrlu_node::operator delete(p, std::nothrow);
}
