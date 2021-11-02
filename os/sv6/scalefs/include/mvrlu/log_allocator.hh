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
    log_allocator() {
      auto log_chunks = new log_space;
      for (log_block &b : log_chunks->space)
        log_pool_.push_front(&b);

      start_addr_ = reinterpret_cast<size_t>(log_chunks);
      end_addr_ = start_addr_ + sizeof(log_block) * MVRLU_MAX_THREAD_NUM;
    }

    void *
    alloc_log_mem() {
      scoped_acquire l(&pool_lock_);

      if (!log_pool_.empty())
      {
        auto& block = log_pool_.front();
        log_pool_.pop_front();
        return block.log_part;
      }
      return nullptr;
    }

    void
    free_log_mem(void *log_part) {
      auto *b = log_block::cast_to_block(log_part);
      if (addr_in_log_region(b))
      {
        scoped_acquire l(&pool_lock_);
        log_pool_.push_front(b);
      }
    }

    bool
    addr_in_log_region(void *addr) {
      size_t addr__ = (size_t) addr;
      return addr__ >= start_addr_ && addr__ < end_addr_;
    }

  };

}
