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

template <>
class list<spinlock> {
  node *head_;
  spinlock lk_;
public:
  list<spinlock>(void) : head_(new node(0)), lk_("spin bench") {}
  ~list<spinlock>(void) {
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
