#pragma once

/*
 * A bucket-chaining hash table.
 */

#include "spinlock.hh"
#include "seqlock.hh"
#include "lockwrap.hh"
#include "hash.hh"
#include "ilist.hh"
#include "hpet.hh"
#include "cpuid.hh"
#include <iterator>

template<class K, class V>
class sorted_chainhash {
private:
  struct item : public rcu_freed {
    item(const K& k, const V& v)
      : rcu_freed("sorted_chainhash::item", this, sizeof(*this)),
        key(k), val(v) {}
    void do_gc() override { delete this; }
    NEW_DELETE_OPS(item);

    islink<item> link;  //ilist.hh -> singly-linked lists
    seqcount<u32> seq;  //seqlock.hh -> a synchronization primitive
    const K key;
    V val;
  };

  struct bucket {
    spinlock lock __mpalign__;
    islist<item, &item::link> chain; //ilist.hh -> intrusive singly-linked list

    ~bucket() {
      while (!chain.empty()) {
        item *i = &chain.front();
        chain.pop_front();
        gc_delayed(i);  //gc.hh 
      }
    }
  };

  u64 nbuckets_;
  bool dead_;
  bucket* buckets_;

public:
  sorted_chainhash(u64 nbuckets) : nbuckets_(nbuckets), dead_(false) {
    buckets_ = new bucket[nbuckets_];
    assert(buckets_);
  }

  ~sorted_chainhash() {
    delete[] buckets_;
  }

  NEW_DELETE_OPS(sorted_chainhash);

  bool insert(const K& k, const V& v) {
    if (dead_ || lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    if (dead_)
      return false;

    auto it = b->chain.before_begin();
    auto prev = it++;
    for (; it != b->chain.end(); prev = it++)
    {
      if (it->key < k)
        continue;
      if (it->key == k)
        return false;
      break;
    }
    b->chain.insert_after(prev, new item(k, v));
    return true;
  }

  bool remove(const K& k, const V& v) {
    if (!lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    auto i = b->chain.before_begin();
    auto end = b->chain.end();
    for (;;) {
      auto prev = i;
      ++i;
      if (i == end || i->key > k)
        return false;
      if (i->key == k && i->val == v) {
        b->chain.erase_after(prev);
        gc_delayed(&*i);
        return true;
      }
    }
  }

  bool remove(const K& k) {
    if (!lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    auto i = b->chain.before_begin();
    auto end = b->chain.end();
    for (;;) {
      auto prev = i;
      ++i;
      if (i == end || i->key > k)
        return false;
      if (i->key == k) {
        b->chain.erase_after(prev);
        gc_delayed(&*i);
        return true;
      }
    }
  }

  int getSize() {
    scoped_gc_epoch rcu_read;

    int size = 0;

    for (u64 i = 0; i < nbuckets_; i++) {
      bucket* b = &buckets_[i];

      for (const item& i: b->chain) {
        (void)i;
        size++;
      }
    }

    return size;
  }

  int getBucketSize()
  {
    return nbuckets_;
  }

  bool lookup(const K& k, V* vptr = nullptr) const {
    scoped_gc_epoch rcu_read;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    for (const item& i: b->chain) {
      if (i.key < k)
        continue;
      if (i.key > k)
        break;
      if (vptr)
        *vptr = *seq_reader<V>(&i.val, &i.seq);
      return true;
    }
    return false;
  }
};
