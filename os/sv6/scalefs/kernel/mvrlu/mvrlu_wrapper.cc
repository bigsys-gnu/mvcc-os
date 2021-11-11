#include "types.h"
#include "mvrlu/mvrlu.hh"
#include "cpu.hh"
#include "proc.hh"

using namespace mvrlu;

thread_handle::thread_handle() {
  self_ = nullptr;
  if (::mvrlu_is_init())
  {
    self_ = ::mvrlu_thread_alloc();
    ::mvrlu_thread_init(self_);
  }
}

thread_handle::~thread_handle(void) {
  if (self_)
  {
    ::mvrlu_thread_finish(self_);
    ::mvrlu_thread_free(self_);
  }
}
