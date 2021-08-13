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
  hash_list<T> &hash_list;
  thread_data<T> &data;

  thread_param(int n_buckets, int nb_threads, int update, int range,
               int &stop, hash_list<T> &hash_list, thread_data<T> &data)
    :n_buckets(n_buckets), nb_threads(nb_threads), update(update),
     range(range), stop(stop), hash_list(hash_list), data(data), variation(0),
     result_add(0), result_remove(0), result_contains(0), result_found(0) {
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

    scoped_acquire(&lk_);
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

    scoped_acquire(&lk_);
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

    scoped_acquire(&lk_);
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

    scoped_acquire(&lk_);
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
      }
    else if(key == cur->value)
      return ret;               // already exists

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
  auto &hash_list = p_data->hash_list;

  cprintf("thread %d Start\n", myproc()->pid);
  // need condition for barrier
  while (p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      bucket = HASH_VALUE(hash_list, value);
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
