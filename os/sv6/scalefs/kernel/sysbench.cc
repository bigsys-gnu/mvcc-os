// SPDX-FileCopyrightText: Copyright (c) 2021 Gyeongsang National University
//
// SPDX-License-Identifier: Apache 2.0 AND MIT

#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "sorted_chainhash.hh"
#include "mvrlu/mvrlu.hh"
#include "chainhash_spinlock.hh"
#include "mvcc_kernel_bench.h"
#include "hash.hh"

#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list.n_buckets)
template <typename T>
struct thread_param;

template <typename T>
void bench(int nb_threads, int initial, int n_buckets, int duration, int update, int range);

template <typename T>
struct hash_list;

template <typename T>
void print_outcome(typename T::data_structure &hl, thread_param<T> *param_list[],
                   int nb_threads, int initial, int duration);

void sleep_usec(u64 initial_time, u64 usec);

struct bench_trait {
  using data_structure = hash_list<bench_trait>;
};
//////////////////////////////////////
// GLOBAL DATA
/////////////////////////////////////
condvar thread_bar("barrier");
spinlock bar_lock("barrier");

void wait_on_barrier(void) {
  scoped_acquire l(&bar_lock);
  thread_bar.sleep(&bar_lock);
}

void wait_and_wakeup(void) {
  sleep_usec(nsectime(), 1000);
  thread_bar.wake_all();
}
//////////////////////////////////////
// SYNCHRONOUS TYPES
/////////////////////////////////////
enum sync_type {
  SPINLOCK = 0,
  MVRLU = 1,
  RCU = 2,
  SPIN_CHAIN = 3
};
//////////////////////////////////////
// RANDOM FUNCTIONS
/////////////////////////////////////
uint64_t
u_rand(void)
{
  uint64_t t = rdtsc() / 2;
  return (t >= 0) ? t : 0 - t;
}

static inline int MarsagliaXORV (int x) { 
  if (x == 0) x = 1 ; 
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ; 
  return x ;        // use either x or x & 0x7FFFFFFF
}

static inline int MarsagliaXOR (int * seed) {
  int x = MarsagliaXORV(*seed);
  *seed = x ; 
  return x & 0x7FFFFFFF;
}

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)u_rand();
  seed[1] = (unsigned short)u_rand();
  seed[2] = (unsigned short)u_rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  int v = MarsagliaXOR((int *)seed) % n;
  return v;
}
////////////////////////////////////////////////////////
int stop = 0;             // shared by threads

struct node {
  node *next;
  int value;

  node(int val) : next(NULL), value(val) {}

  NEW_DELETE_OPS(node);
};

template <typename T>
class list {};

template <typename T>
class hash_list {
  int n_buckets_;
  list<T> **buckets_;
public:
  hash_list(int n_buckets): n_buckets_(n_buckets)
  {
    buckets_ = new list<T> *[n_buckets];
    for (int i = 0; i < n_buckets; i++)
      buckets_[i] = new list<T>();
  }
  ~hash_list(void) {
    for (int i = 0; i < n_buckets_; i++)
      {
        delete buckets_[i];
      }
    delete[] buckets_;
  }

  list<T> *
  get_list(int key) {
    return buckets_[hash(key) % n_buckets_];
  }

  // only for initialization
  bool
  raw_insert(int key) {
    auto list = get_list(key);
    return list->raw_insert(key);
  }

  unsigned long
  get_total_node_num(void) {
    unsigned long total = 0;
    for (int i = 0; i < n_buckets_; i++)
      total += buckets_[i]->get_total_node_num();
    return total;
  }

  NEW_DELETE_OPS(hash_list<T>);
};

template <typename T>
struct thread_param {
  int n_buckets;
  int nb_threads;
  int update;
  int range;
  unsigned long variation;
  unsigned long result_add;
  unsigned long result_remove;
  unsigned long result_contains;
  unsigned long result_found;
  int &stop;
  unsigned short seed[3];
  typename T::data_structure *hl;

  thread_param(int n_buckets, int nb_threads, int update, int range,
               int &stop, typename T::data_structure *hl)
    :n_buckets(n_buckets), nb_threads(nb_threads), update(update),
     range(range), variation(0), result_add(0), result_remove(0),
     result_contains(0), result_found(0), stop(stop), hl(hl) {
    rand_init(seed);
  }

  NEW_DELETE_OPS(thread_param<T>);
};
template <typename T>
void test(void *param) {}

template <typename T>
void bench_init(void) {}

template <typename T>
void bench_finish(void) {}

//////////////////////////////////////
// SPINLOCK START
/////////////////////////////////////
struct spinlock_bench: public bench_trait {
  using data_structure = hash_list<spinlock_bench>;
};

template <>
class list<spinlock_bench> {
  node *head_;
  spinlock lk_;
public:
  list(void) : head_(new node(0)), lk_("spin bench") {}
  ~list(void) {
    for (auto iter = head_; iter != NULL;)
    {
      auto trash = iter;
      iter = iter->next;
      delete trash;
    }
  }

  int list_insert(int key) {
    node *prev, *cur;
    int ret = 0;

    scoped_acquire l(&lk_);
    for (prev = head_, cur = prev->next; ; prev = cur, cur = cur->next)
      {
        if (cur == NULL || cur->value > key)
          {
            auto new_node = new node(key);
            new_node->next = prev->next;
            prev->next = new_node;
            ret = 1;            // successed
            break;
          }
        else if (cur->value == key)
          break;                // the key value already exists.
      }
    return ret;
  }

  int list_delete(int key) {
    node *prev, *cur;
    int ret = 0;

    scoped_acquire l(&lk_);
    for (prev = head_, cur = prev->next; cur != NULL; prev = cur, cur = prev->next)
      {
        // found the target to be deleted
        if (cur->value == key)
          {
            prev->next = cur->next;
            delete cur;
            ret = 1;
            break;
          }
      }
    return ret;
  }

  int list_find(int key) {
    node *cur;
    int value = -1;

    scoped_acquire l(&lk_);
    cur = head_;
    while (cur && cur->value < key)
      cur = cur->next;

    // found the value
    if (cur && cur->value == key)
      value = cur->value;

    return value;
  }

  int raw_insert(int key) {
    node *prev, *cur;
    int ret = 0;

    for (prev = head_, cur = prev->next; cur != NULL;
         prev = cur, cur = cur->next)
      {
        if (key < cur->value)
          {
            ret = 1;
            auto new_node = new node(key);
            new_node->next = cur;
            prev->next = new_node;
            return ret;
          }
        else if(key == cur->value)
          return ret;               // already exists
      }

    if (cur == NULL)
      {
        ret = 1;
        auto new_node = new node(key);
        new_node->next = cur;
        prev->next = new_node;
      }
    return ret;
  }

  int get_total_node_num(void) {
    int total_num = 0;
    for (auto iter = head_->next; iter != NULL; iter = iter->next)
    {
      total_num++;
    }
    return total_num;
  }

  NEW_DELETE_OPS(list<spinlock_bench>);
};

template <>
void test<spinlock_bench>(void *param) {
  int op, value;
  auto *p_data = reinterpret_cast<thread_param<spinlock_bench> *>(param);
  auto &hash_list = *p_data->hl;

  wait_on_barrier();

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      auto *p_list = hash_list.get_list(value);

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (p_list->list_insert(value))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (p_list->list_delete(value))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(p_list->list_find(value) >= 0)
            {
              p_data->result_found++;
            }
           p_data->result_contains++;
        }
    }
  cprintf("thread %d end\n", myproc()->pid);
}
//////////////////////////////////////
// SPINLOCK END
/////////////////////////////////////
//////////////////////////////////////
// RCU START (RCU + SEQLOCK)
/////////////////////////////////////
struct rcu_hash_list;
struct rcu_bench {
  using data_structure = struct rcu_hash_list;
};

template <>
unsigned long hash<int>(int const& k) {
  return (unsigned long)k;
}

// rcu hash list should not have any dynamic allocated data
struct rcu_hash_list : public sorted_chainhash<int, int> {
  rcu_hash_list(u64 nbuckets): sorted_chainhash<int, int>(nbuckets) {}

  int get_total_node_num(void) {
    return getSize();
  }

  int raw_insert(int key) {
    return insert(key, key);
  }

  NEW_DELETE_OPS(rcu_hash_list);
};

template <>
void test<rcu_bench>(void *param) {
  int op, value;
  auto *p_data = reinterpret_cast<thread_param<rcu_bench> *>(param);
  auto &hash_list = *p_data->hl;

  wait_on_barrier();

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (hash_list.insert(value, value))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (hash_list.remove(value, value))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(hash_list.lookup(value))
            {
              p_data->result_found++;
            }
          p_data->result_contains++;
        }
    }
  cprintf("thread %d end\n", myproc()->pid);
}
//////////////////////////////////////
// RCU END
/////////////////////////////////////
//////////////////////////////////////
// SPIN CHAIN START
/////////////////////////////////////
struct spin_chain : public chainhash_spinlock<int, int> {
  spin_chain(u64 nbuckets): chainhash_spinlock<int, int>(nbuckets) {}

  int get_total_node_num(void) {
    int total_node_num = 0;
    enumerate([&total_node_num](int, int){
      total_node_num++;
      return false;
    });
    return total_node_num;
  }

  int raw_insert(int key) {
    return insert(key, key);
  }

  NEW_DELETE_OPS(spin_chain);
};

struct spin_chain_bench: public bench_trait {
  using data_structure = spin_chain;
};

template <>
void test<spin_chain_bench>(void *param) {
  int op, value;
  auto *p_data = reinterpret_cast<thread_param<spin_chain_bench> *>(param);
  auto &hash_list = *p_data->hl;

  wait_on_barrier();

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (hash_list.insert(value, value))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (hash_list.remove(value, value))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(hash_list.lookup(value))
            {
              p_data->result_found++;
            }
          p_data->result_contains++;
        }
    }
  cprintf("thread %d end\n", myproc()->pid);
}

//////////////////////////////////////
// SPIN CHAIN END
/////////////////////////////////////
//////////////////////////////////////
// MVRLU START
/////////////////////////////////////
struct mvrlu_bench: public bench_trait {
  using data_structure = hash_list<mvrlu_bench>;
};

struct mvrlu_node {
  int value;
  mvrlu_node *next;

  mvrlu_node(int val): value(val), next(NULL) {}

  MVRLU_NEW_DELETE(mvrlu_node);
};

template <>
class list<mvrlu_bench> {
  mvrlu_node *head_;
public:
  list(void):head_(new mvrlu_node(0)) {}
  ~list(void) {
    while (pop());
  }

  int pop(void) {
    mvrlu_node *iter;
    mvrlu::thread_handle &h = myproc()->handle;

  restart:
    h.mvrlu_reader_lock();
    iter = h.mvrlu_deref(head_->next);
    if (!iter)
    {
      if (!h.mvrlu_try_lock(&head_))
      {
        h.mvrlu_abort();
        goto restart;
      }
      h.mvrlu_free(head_);
      head_ = nullptr;
      return 0;
    }
    if (!h.mvrlu_try_lock(&head_) ||
        !h.mvrlu_try_lock_const(iter))
    {
      h.mvrlu_abort();
      goto restart;
    }
    mvrlu::mvrlu_assign_pointer(&head_->next, iter->next);
    h.mvrlu_free(iter);
    h.mvrlu_reader_unlock();
    return 1;
  }

  int list_insert(int key) {
    mvrlu_node *prev, *cur;
    int ret = 0;
    mvrlu::thread_handle &h = myproc()->handle;

  restart:
    h.mvrlu_reader_lock();
    for (prev = h.mvrlu_deref(head_), cur = h.mvrlu_deref(prev->next); ;
         prev = cur, cur = h.mvrlu_deref(cur->next))
    {
      if (cur == NULL || cur->value > key)
      {
        if (!h.mvrlu_try_lock(&prev) ||
            !h.mvrlu_try_lock(&cur))
        {
          h.mvrlu_abort();
          goto restart;
        }
        auto new_node = new mvrlu_node(key);
        mvrlu::mvrlu_assign_pointer(&new_node->next, cur);
        mvrlu::mvrlu_assign_pointer(&prev->next, new_node);
        ret = 1;
        break;
      }
      else if (cur->value == key)
        break;
    }
    h.mvrlu_reader_unlock();
    return ret;
  }

  int list_delete(int key) {
    mvrlu_node *prev, *cur;
    int ret = 0;
    mvrlu::thread_handle &h = myproc()->handle;

  restart:
    h.mvrlu_reader_lock();
    for (prev = h.mvrlu_deref(head_), cur = h.mvrlu_deref(prev->next); cur != NULL;
         prev = cur, cur = h.mvrlu_deref(cur->next))
    {
      if (cur->value == key)
      {
        if (!h.mvrlu_try_lock(&prev) ||
            !h.mvrlu_try_lock_const(cur))
        {
          h.mvrlu_abort();
          goto restart;
        }
        auto *cur_n = h.mvrlu_deref(cur->next);
        mvrlu::mvrlu_assign_pointer(&prev->next, cur_n);
        h.mvrlu_free(cur);
        ret = 1;
        break;
      }
    }
    h.mvrlu_reader_unlock();
    return ret;
  }

  int list_find(int key) {
    int value = -1;
    mvrlu::thread_handle &h = myproc()->handle;

    h.mvrlu_reader_lock();
    auto *cur = h.mvrlu_deref(head_);

    while (cur && cur->value < key)
      cur = h.mvrlu_deref(cur->next);

    if (cur && cur->value == key)
      value = cur->value;

    h.mvrlu_reader_unlock();
    return value;
  }

  int raw_insert(int key) {
    mvrlu_node *prev, *cur;
    int ret = 0;

    for (prev = head_, cur = prev->next; cur != NULL;
         prev = cur, cur = cur->next)
      {
        if (key < cur->value)
          {
            ret = 1;
            auto new_node = new mvrlu_node(key);
            new_node->next = cur;
            prev->next = new_node;
            return ret;
          }
        else if(key == cur->value)
          return ret;               // already exists
      }

    if (cur == NULL)
      {
        ret = 1;
        auto new_node = new mvrlu_node(key);
        new_node->next = cur;
        prev->next = new_node;
      }
    return ret;
  }

  int get_total_node_num(void) {
    int total_num = 0;
    mvrlu::thread_handle &h = myproc()->handle;
    mvrlu_node *iter;

    h.mvrlu_reader_lock();
    for (iter = h.mvrlu_deref(head_->next); iter != nullptr;
         iter = h.mvrlu_deref(iter->next))
    {
      total_num++;
    }
    h.mvrlu_reader_unlock();
    return total_num;
  }

  NEW_DELETE_OPS(list<mvrlu_bench>);
};

template <>
void test<mvrlu_bench>(void *param) {
  int op, value;
  auto *p_data = reinterpret_cast<thread_param<mvrlu_bench> *>(param);
  auto &hash_list = *p_data->hl;

  wait_on_barrier();

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      auto *p_list = hash_list.get_list(value);

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (p_list->list_insert(value))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (p_list->list_delete(value))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(p_list->list_find(value) >= 0)
            {
              p_data->result_found++;
            }
          p_data->result_contains++;
        }
    }
  myproc()->handle.mvrlu_flush_log();
  cprintf("thread %d end\n", myproc()->pid);
}
//////////////////////////////////////
// MVRLU FINISH
/////////////////////////////////////
template <typename T>
void bench(int nb_threads, int initial, int n_buckets, int duration, int update,
           int range, kernel_bench_outcome *out)
{
  bench_init<T>();

  auto *hl = new typename T::data_structure(n_buckets);

  cprintf("initialize %d nodes...", initial);
  int i = 0;
  unsigned short seed[3];

  rand_init(seed);
  while (i < initial)
  {
    int value = rand_range(range, seed);
    if (hl->raw_insert(value))
      i++;
  }
  cprintf("done\n");

  // allocate pointer list
  struct proc **thread_list = new struct proc *[nb_threads];
  thread_param<T> **param_list = new thread_param<T> *[nb_threads];

  // mile sec
  cprintf("Main thread ID: %d\n", myproc()->pid);
  cprintf("Creating %d threads...", nb_threads);

  for (int i = 0; i < nb_threads; i++)
  {
    param_list[i] = new thread_param<T>(n_buckets, nb_threads, update,
                                               range, stop, hl);
  }
  for (int i = 0; i < nb_threads; i++)
  {
    thread_list[i] = threadpin(test<T>, (void*)param_list[i], "test_thread",
                               i%(NCPU-1)+1);
    cprintf("\nThread created %p(c:%d, s:%d)\n", thread_list[i], i%(NCPU-1)+1,
            thread_list[i]->get_state());
  }

  wait_and_wakeup();

  cprintf("\nstart bench!\n");

  sleep_usec(nsectime(), duration);

  stop = 1;
  cprintf("join %d threads...\n", nb_threads);

  sleep_usec(nsectime(), 4000); // wait for threads

  bench_finish<T>();
  cprintf(" done!\n");

  print_outcome<T>(*hl, param_list, nb_threads, initial, duration, out);

  // deallocate memory
  delete hl;
  for (int j = 0; j < nb_threads; j++)
  {
    delete param_list[j];
  }
  delete[] thread_list;
  delete[] param_list;
}

//SYSCALL
void
sys_benchmark(void *arguments, void *retptr)
{
  assert(arguments != nullptr);
  assert(retptr != nullptr);

  kernel_bench_args *args = reinterpret_cast<kernel_bench_args *>(arguments);
  int nb_threads = args->nb_threads;
  int initial = args->initial;
  int n_buckets = args->n_buckets;
  int duration = args->duration;
  int update = args->update;
  int range = args->range;
  enum sync_type type = (enum sync_type) args->sync_type;
  cprintf("Run Kernel Level Benchmark\n");

  assert(n_buckets >= 1);
  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(update >= 0 && update <= 1000);
  assert(range > 0 && range >= initial);

  switch (type) {
  case SPINLOCK:
    bench<spinlock_bench>(nb_threads, initial, n_buckets, duration,
                          update, range, reinterpret_cast<kernel_bench_outcome *>(retptr));
    break;
  case MVRLU:
    bench<mvrlu_bench>(nb_threads, initial, n_buckets, duration,
                          update, range, reinterpret_cast<kernel_bench_outcome *>(retptr));
    break;
  case RCU:
    bench<rcu_bench>(nb_threads, initial, n_buckets, duration,
                          update, range, reinterpret_cast<kernel_bench_outcome *>(retptr));
    break;
  case SPIN_CHAIN:
    bench<spin_chain_bench>(nb_threads, initial, n_buckets, duration,
                            update, range, reinterpret_cast<kernel_bench_outcome *>(retptr));
    break;
  default:
    cprintf("Wrong sync type! 0:spinlock 1:mvrlu 2:rcu+seqlock\n");
  }
  stop = 0;
  cprintf("Kernel Level Benchmark END\n");
}
//////////////////////////////////////
// NO DEPENDENT CODE
/////////////////////////////////////
// initial_time is nano sec
void sleep_usec(u64 initial_time, u64 usec) {
  spinlock l("l");
  condvar cond("sleep cond");
  u64 until = usec * 1000000 + initial_time;

  scoped_acquire local(&l);
  if (until > nsectime())
    cond.sleep_to(&l, until);
}

template <typename T>
void print_outcome(typename T::data_structure &hl, thread_param<T> *param_list[],
                   int nb_threads, int initial, int duration,
                   kernel_bench_outcome *retptr) {
  unsigned long reads = 0, updates = 0, total_variation = 0;

  for (int i = 0; i < nb_threads; i++)
    {
      cprintf( "Thread %d\n", i);
      cprintf( "  #add        : %lu\n", param_list[i]->result_add);
      cprintf( "  #remove     : %lu\n", param_list[i]->result_remove);
      cprintf( "  #contains   : %lu\n", param_list[i]->result_contains);
      cprintf( "  #found      : %lu\n", param_list[i]->result_found);
      reads += param_list[i]->result_contains;
      updates += (param_list[i]->result_add + param_list[i]->result_remove);
      total_variation += param_list[i]->variation;
    }
  unsigned long total_size = hl.get_total_node_num();

  unsigned long exp = initial + total_variation;

  retptr->total_size = total_size;
  retptr->exp = exp;
  retptr->total_read = reads;
  retptr->total_update = updates;
}
