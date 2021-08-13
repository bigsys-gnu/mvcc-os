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
  list<spinlock>(void) : head_(NULL), lk_("spin bench") {}
  ~list<spinlock>(void) {
    for (auto iter = head_; iter != NULL;)
    {
      auto trash = iter;
      iter = iter->next;
      delete trash;
    }
  }

  int list_insert(int key) {
    return 0;
  }

  int list_delete(int key) {
    return 0;
  }

  int list_find(int key) {
    return 0;
  }

  int raw_insert(int key) {
    return 0;
  }
  
  NEW_DELETE_OPS(list<spinlock>);
};
