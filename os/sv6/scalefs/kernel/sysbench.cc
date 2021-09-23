#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "mmu.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "vm.hh"
#include "kmtrace.hh"
#include "futex.h"
#include "version.hh"
#include "filetable.hh"
#include "mvrlu/mvrlu.hpp"
#include "chainhash.hh"

#include <uk/mman.h>
#include <uk/utsname.h>
#include <uk/unistd.h>

#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list.n_buckets)
template <typename T>
struct thread_param;
template <typename T>
void bench(int nb_threads, int initial, int n_buckets, int duration, int update, int range);
template <typename T>
struct hash_list;

template <typename T>
unsigned long
get_total_node_num(hash_list<T> &hl);

template <typename T>
void print_outcome(typename T::data_structure &hl, thread_param<T> *param_list[],
                   int nb_threads, int initial, int duration);
void sleep_usec(u64 initial_time, u64 usec);

struct bench_trait {
  using data_structure = hash_list<bench_trait>;
};
//////////////////////////////////////
// SYNCHRONOUS TYPES
/////////////////////////////////////
enum sync_type {
  SPINLOCK = 0,
  MVRLU = 1,
  RCU = 2
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
class list {
  int list_insert(T& data, int key) { return 0; }
  int list_delete(T& data, int key) { return 0; }
  int list_find(T& data, int key) { return 0; }
  int get_total_node_number(void) { return 0; }
};

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
    return buckets_[key];
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
  int op, bucket, value;
  auto *p_data = reinterpret_cast<thread_param<spinlock_bench> *>(param);
  auto &hash_list = *p_data->hl;

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      bucket = value % p_data->n_buckets;
      auto *p_list = hash_list.get_list(bucket);

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
              p_data->result_contains++;
            }
          p_data->result_found++;
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
struct rcu_hash_list : public chainhash<int, int> {
  rcu_hash_list(u64 nbuckets): chainhash<int, int>(nbuckets) {}

  int get_total_node_num(void) {
    total_node_num_ = 0;
    enumerate([this](int, int){
      this->total_node_num_++;
      return false;
    });
    return total_node_num_;
  }

  int raw_insert(int key) {
    return insert(key, key);
  }

  NEW_DELETE_OPS(rcu_hash_list);
private:
  int total_node_num_;
};

template <>
void test<rcu_bench>(void *param) {
  int op, value;
  auto *p_data = reinterpret_cast<thread_param<rcu_bench> *>(param);
  auto &hash_list = *p_data->hl;

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
              p_data->result_contains++;
            }
          p_data->result_found++;
        }
    }
  cprintf("thread %d end\n", myproc()->pid);
}
//////////////////////////////////////
// RCU END
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

  static void* operator new(unsigned long nbytes, const std::nothrow_t&) noexcept {
    return mvrlu::mvrlu_alloc<mvrlu_node>();
  }

  static void* operator new(unsigned long nbytes) {
    void *p = mvrlu_node::operator new(nbytes, std::nothrow);
    if (p == nullptr)
      throw_bad_alloc();
    return p;
  }

  static void operator delete(void *p, const std::nothrow_t&) noexcept {
    mvrlu::mvrlu_free(p);
  }

  static void operator delete(void *p) {
    mvrlu_node::operator delete(p, std::nothrow);
  }

};

template <>
class list<mvrlu_bench> {
  mvrlu_node *head_;
public:
  list(void):head_(new mvrlu_node(0)) {}
  ~list(void) {
    for (auto iter = head_; iter != NULL; )
    {
      auto trash = iter;
      iter = iter->next;
      delete trash;
    }
  }

  int list_insert(mvrlu::thread_handle<mvrlu_node> &h, int key) {
    mvrlu_node *prev, *cur;
    int ret = 0;

  restart:
    h.mvrlu_reader_lock();
    for (prev = h.mvrlu_deref(head_), cur = h.mvrlu_deref(prev->next); ;
         prev = cur, cur = h.mvrlu_deref(cur->next))
    {
      if (cur == NULL || cur->value > key)
      {
        if (!h.mvrlu_try_lock(&prev))
        {
          h.mvrlu_abort();
          goto restart;
        }
        auto new_node = new mvrlu_node(key);
        mvrlu::mvrlu_assign_pointer(&new_node->next, prev->next);
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

  int list_delete(mvrlu::thread_handle<mvrlu_node> &h, int key) {
    mvrlu_node *prev, *cur;
    int ret = 0;

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

  int list_find(mvrlu::thread_handle<mvrlu_node> &h, int key) {
    int value = -1;

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
    for (auto iter = head_->next; iter != NULL; iter = iter->next)
      total_num++;
    return total_num;
  }

  NEW_DELETE_OPS(list<mvrlu_bench>);
};

template <>
void bench_init<mvrlu_bench>(void) {
  mvrlu_init();
}

template <>
void bench_finish<mvrlu_bench>(void) {
  mvrlu_finish();
}

template <>
void test<mvrlu_bench>(void *param) {
  int op, bucket, value;
  auto *p_data = reinterpret_cast<thread_param<mvrlu_bench> *>(param);
  auto &hash_list = *p_data->hl;
  auto *handle = new mvrlu::thread_handle<mvrlu_node>();

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      bucket = value % p_data->n_buckets;
      auto *p_list = hash_list.get_list(bucket);

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (p_list->list_insert(*handle, value))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (p_list->list_delete(*handle, value))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(p_list->list_find(*handle, value) >= 0)
            {
              p_data->result_contains++;
            }
          p_data->result_found++;
        }
    }
  delete handle;
  cprintf("thread %d end\n", myproc()->pid);
}
//////////////////////////////////////
// MVRLU FINISH
/////////////////////////////////////
template <typename T>
void bench(int nb_threads, int initial, int n_buckets, int duration, int update,
           int range)
{
  bench_init<T>();

  // hash_list<T> *hl = new hash_list<T>(n_buckets);
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
  u64 initial_time = nsectime();
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
  cprintf(" done!\n");

  sleep_usec(initial_time, duration);

  stop = 1;
  cprintf("join %d threads...\n", nb_threads);

  sleep_usec(nsectime(), 4000); // wait for threads

  bench_finish<T>();
  cprintf(" done!\n");

  print_outcome<T>(*hl, param_list, nb_threads, initial, duration);

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
sys_benchmark(int nb_threads, int initial, int n_buckets, int duration, int update,
              int sync_type)
{
  enum sync_type type = (enum sync_type) sync_type;
  cprintf("Run Kernel Level Benchmark\n");
  int range = initial * 2;

  assert(n_buckets >= 1);
  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(update >= 0 && update <= 1000);
  // assert(range > 0 && range >= initial);

  switch (type) {
  case SPINLOCK:
    bench<spinlock_bench>(nb_threads, initial, n_buckets, duration, update, range);
    cprintf("spinlock\n");
    break;
  case MVRLU:
    bench<mvrlu_bench>(nb_threads, initial, n_buckets, duration, update, range);
    cprintf("mvrlu\n");
    break;
  case RCU:
    bench<rcu_bench>(nb_threads, initial, n_buckets, duration, update, range);
    cprintf("rcu + seqlock\n");
    break;
  default:
    cprintf("Wrong sync type!\n");
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
unsigned long
get_total_node_num(typename T::data_structure &hl)
{
  unsigned long total_size = hl.get_total_node_num();
  return total_size;
}

template <typename T>
void print_outcome(typename T::data_structure &hl, thread_param<T> *param_list[],
                   int nb_threads, int initial, int duration) {
  unsigned long reads = 0, updates = 0, total_variation = 0;

  for (int i = 0; i < nb_threads; i++)
    {
      cprintf( "Thread %d\n", i);
      cprintf( "  #add        : %lu\n", param_list[i]->result_add);
      cprintf( "  #remove     : %lu\n", param_list[i]->result_remove);
      cprintf( "  #contains   : %lu\n", param_list[i]->result_contains);
      cprintf( "  #found      : %lu\n", param_list[i]->result_found);
      reads += param_list[i]->result_found;
      updates += (param_list[i]->result_add + param_list[i]->result_remove);
      total_variation += param_list[i]->variation;
    }
  unsigned long total_size = hl.get_total_node_num();

  unsigned long exp = initial + total_variation;
  cprintf( "\n#### B ####\n");

  cprintf( "Set size      : %lu (expected: %lu)\n", total_size, exp);
  cprintf( "Duration      : %d (ms)\n", duration);
  unsigned long iv = reads * 1000.0 / duration;
  unsigned long fv = (unsigned long)(reads * 1000.0 / duration * 10) % 10;
  cprintf( "#read ops     : %lu (%lu.%lu / s)\n", reads, iv, fv);
  iv = updates * 1000.0 / duration;
  fv = (unsigned long)(updates * 1000.0 / duration * 10) % 10;
  cprintf( "#update ops   : %lu (%lu.%lu / s)\n", updates, iv, fv);

  if(exp != total_size)
  {
    cprintf("\n<<<<<< ASSERT FAILURE(%lu!=%lu) <<<<<<<<\n", exp, total_size);
  }
}
