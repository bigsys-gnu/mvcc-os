#pragma once

#include "mvrlu/mvrlu.hh"
#include "cpu.hh"
#include "proc.hh"

namespace mvrlu {
  class mvrlu_section {
  public:
    mvrlu_section(void)
    {
      myproc()->handle.mvrlu_reader_lock();
    }
    ~mvrlu_section(void)
    {
      myproc()->handle.mvrlu_reader_unlock();
    }
  };
}
