#pragma once

/*
 * A bucket-chaining hash table using spinlock instead of RCU.
 * Forked from include/chainhash.hh
 */

#include "spinlock.hh"
#include "seqlock.hh"
#include "lockwrap.hh"
#include "hash.hh"
#include "ilist.hh"
#include "hpet.hh"
#include "cpuid.hh"

template<class K, class V>
class chainhash_spinlock {
private:
  struct item {
    item(const K& k, const V& v)
    : key(k), val(v) {}
    NEW_DELETE_OPS(item);

    islink<item> link;
    seqcount<u32> seq;
    const K key;
    V val;
  };

  struct bucket {
    spinlock lock __mpalign__;
    islist<item, &item::link> chain;

    ~bucket() {
      while (!chain.empty()) {
        chain.pop_front();
      }
    }
  };

  u64 nbuckets_;
  bool dead_;
  bucket* buckets_;

public:
  chainhash_spinlock(u64 nbuckets) : nbuckets_(nbuckets), dead_(false) {
    buckets_ = new bucket[nbuckets_];
    assert(buckets_);
  }

  ~chainhash_spinlock() {
    delete[] buckets_;
  }

  NEW_DELETE_OPS(chainhash_spinlock);

  bool insert(const K& k, const V& v) {
    if (dead_ || lookup(k))
      return false;

    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock);

    if (dead_)
      return false;

    for (const item& i: b->chain)
      if (i.key == k)
        return false;

    b->chain.push_front(new item(k, v));
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
      if (i == end)
        return false;
      if (i->key == k && i->val == v) {
        b->chain.erase_after(prev);
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
      if (i == end)
        return false;
      if (i->key == k) {
        b->chain.erase_after(prev);
        return true;
      }
    }
  }

  bool enumerate(const K* prev, K* out) const {
    
    bool prevbucket = (prev != nullptr);
    for (u64 i = prev ? hash(*prev) % nbuckets_ : 0; i < nbuckets_; i++) {
      bucket* b = &buckets_[i];
      scoped_acquire l(&b->lock); // modify RCU to spinlock
      bool found = false;
      for (const item& i: b->chain) {
        if ((!prevbucket || *prev < i.key) && (!found || i.key < *out)) {
          *out = i.key;
          found = true;
        }
      }
      if (found)
        return true;
      prevbucket = false;
    }

    return false;
  }

  template<class CB>
  void enumerate(CB cb) const {

    for (u64 i = 0; i < nbuckets_; i++) {
      bucket* b = &buckets_[i];
      scoped_acquire l(&b->lock); // modify RCU to spinlock

      for (const item& i: b->chain) {
        V val = *seq_reader<V>(&i.val, &i.seq);
        if (cb(i.key, val))
          return;
      }
    }
  }

  bool lookup(const K& k, V* vptr = nullptr) const {
    bucket* b = &buckets_[hash(k) % nbuckets_];
    scoped_acquire l(&b->lock); // modify RCU to spinlock

    for (const item& i: b->chain) {
      if (i.key != k)
        continue;
      if (vptr)
        *vptr = *seq_reader<V>(&i.val, &i.seq);
      return true;
    }
    return false;
  }

  bool killed() const {
    return dead_;
  }
};
