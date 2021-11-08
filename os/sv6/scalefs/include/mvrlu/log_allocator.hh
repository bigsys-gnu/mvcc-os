#pragma once

#include "cpputil.hh"
#include "kernel.hh"
#include "ilist.hh"
#include "spinlock.hh"
#include "mvrlu/config.h"
#include <cstddef>


namespace mvrlu {

  class log_allocator {
    struct log_block {
      islink<log_block> next;
      char log_part[MVRLU_LOG_SIZE];

      // this code is resamble with containter_from_member
      static log_block *
      cast_to_block(void *log_reg) {
        log_block *c = nullptr;
        const char *c_mem = reinterpret_cast<const char *>(&(c->log_part));
        auto offset = c_mem - reinterpret_cast<const char *>(c);
        return (log_block *) ((char *)log_reg - offset);
      }
    };

    struct log_space {
      log_block space[MVRLU_MAX_THREAD_NUM];
      NEW_DELETE_OPS(log_space);
    };

    islist<log_block, &log_block::next> log_pool_;
    spinlock pool_lock_;
    size_t start_addr_;
    size_t end_addr_;

  public:
    log_allocator();

    void * alloc_log_mem();

    void free_log_mem(void *log_part);

    bool
    addr_in_log_region(void *addr) {
      size_t addr__ = (size_t) addr;
      return addr__ >= start_addr_ && addr__ < end_addr_;
    }

  };

}
