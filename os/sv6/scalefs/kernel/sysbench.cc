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

#include <uk/mman.h>
#include <uk/utsname.h>
#include <uk/unistd.h>

#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list.n_buckets)
//////////////////////////////////////
// BENCHMARK TYPES
/////////////////////////////////////
enum bench_type {
  SPINLOCK
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
};

template <typename T>
class hash_list {
  int n_buckets_;
  list<T> **buckets_;
};

template <typename T>
struct thread_data {};

template <typename T>
struct thread_param {
  int n_buckets;
  int nb_threads;
  int update;
  int range;
  int variation;
  int result_add;
  int result_remove;
  int result_contains;
  int result_found;
  int &stop;
  unsigned short seed[3];
  hash_list<T> &hl;
  thread_data<T> &data;

  thread_param(int n_buckets, int nb_threads, int update, int range,
               int &stop, hash_list<T> &hl, thread_data<T> &data)
    :n_buckets(n_buckets), nb_threads(nb_threads), update(update),
     range(range), variation(0), result_add(0), result_remove(0),
     result_contains(0), result_found(0), stop(stop), hl(hl), data(data) {
    rand_init(seed);
  }

  NEW_DELETE_OPS(thread_param<T>);
};
template <typename T>
void test(void *param) {}

template <>
class list<spinlock> {
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
  
  NEW_DELETE_OPS(list<spinlock>);
};

template <>
class hash_list<spinlock> {
  int n_buckets_;
  list<spinlock> **buckets_;
public:
  hash_list(int n_buckets): n_buckets_(n_buckets)
  {

    buckets_ = new list<spinlock> *[n_buckets];
    for (int i = 0; i < n_buckets; i++)
      buckets_[i] = new list<spinlock>();
  }
  ~hash_list(void) {
    for (int i = 0; i < n_buckets_; i++)
      {
        delete buckets_[i];
      }
    delete[] buckets_;
  }

  list<spinlock> *
  get_list(int key) {
    return buckets_[key];
  }

  NEW_DELETE_OPS(hash_list<spinlock>);
};

template <>
void test<spinlock>(void *param) {
  int op, bucket, value;
  auto *p_data = reinterpret_cast<thread_param<spinlock> *>(param);
  auto &hash_list = p_data->hl;

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

void sleep_usec(u64 initial_time, u64 usec);

template <typename T>
void print_outcome(hash_list<T> &hl, thread_param<T> *param_list[], size_t len);

//SYSCALL
void
sys_benchmark(int nb_threads, int initial, int n_buckets, int duration, int update,
              int range)
{
  enum bench_type type = SPINLOCK;
  cprintf("Run Kernel Level Benchmark\n");

  assert(n_buckets >= 1);
  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(update >= 0 && update <= 1000);
  assert(range > 0 && range >= initial);

  if (type == SPINLOCK)
    {
      hash_list<spinlock> *hl = new hash_list<spinlock>(n_buckets);

      cprintf("initialize %d nodes...", initial);
      int i = 0;
      unsigned short seed[3];

      rand_init(seed);
      while (i < initial)
        {
          int value = rand_range(range, seed);
          auto list = hl->get_list(value % n_buckets);

          if (list->raw_insert(value))
            i++;
        }
      cprintf("done\n");

      // allocate pointer list
      struct proc **thread_list = new struct proc *[nb_threads];
      thread_param<spinlock> **param_list = new thread_param<spinlock> *[nb_threads];

      // mile sec
      u64 initial_time = nsectime();
      cprintf("Main thread ID: %d\n", myproc()->pid);
      cprintf("Creating %d threads...", nb_threads);
      int stop = 0;             // shared by threads
      thread_data<spinlock> d;  // spinlock need no data. this is just shell.
      for (int i = 0; i < nb_threads; i++)
        {
          param_list[i] = new thread_param<spinlock>(n_buckets, nb_threads, update,
                                                     range, stop, *hl, d);
        }
      for (int i = 0; i < nb_threads; i++)
        {
        thread_list[i] = threadpin(test<spinlock>, (void*)param_list[i], "test_thread",
                                   i%(NCPU-1)+1);
        cprintf("\nThread created %p(c:%d, s:%d)\n", thread_list[i], i%(NCPU-1)+1,
                thread_list[i]->get_state());
        }
      cprintf(" done!\n");

      sleep_usec(initial_time, duration);

      stop = 1;
      cprintf("join %d threads...\n", nb_threads);
      for(int i = 0; i < nb_threads; i++)
        wait(-1, NULL);

      cprintf(" done!\n");

      print_outcome<spinlock>(*hl, param_list, nb_threads);

      // deallocate memory
      delete hl;
      for (int j = 0; j < nb_threads; j++)
        {
          delete param_list[j];
        }
      delete[] thread_list;
      delete[] param_list;
    } // spinlock
  
}

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
void print_outcome(hash_list<T> &hl, thread_param<T> *param_list[], size_t len) {
  int reads, updates, total_variation;

  for (int i = 0; i < len; i++)
    {
      cprintf( "Thread %d\n", i);
      cprintf( "  #add        : %d\n", param_list[i]->result_add);
      cprintf( "  #remove     : %d\n", param_list[i]->result_remove);
      cprintf( "  #contains   : %d\n", param_list[i]->result_contains);
      cprintf( "  #found      : %d\n", param_list[i]->result_found);
      reads += param_list[i]->result_found;
      updates += (param_list[i]->result_add + param_list[i]->result_remove);
      total_variation += param_list[i]->variation;
    }
}
