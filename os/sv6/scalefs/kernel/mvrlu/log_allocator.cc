#include "types.h"
#include "mvrlu/log_allocator.hh"
#include "cpu.hh"

using namespace mvrlu;

log_allocator::log_allocator() {
  auto log_chunks = new log_space;
  for (log_block &b : log_chunks->space)
    log_pool_.push_front(&b);

  start_addr_ = reinterpret_cast<size_t>(log_chunks);
  end_addr_ = start_addr_ + sizeof(log_block) * (MVRLU_MAX_THREAD_NUM);
}

void *
log_allocator::alloc_log_mem() {
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
log_allocator::free_log_mem(void *log_part) {
  auto *b = log_block::cast_to_block(log_part);
  if (addr_in_log_region(b))
  {
    scoped_acquire l(&pool_lock_);
    log_pool_.push_front(b);
    return;
  }
  panic("free failed");
}
