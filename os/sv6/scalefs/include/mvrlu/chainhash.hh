#pragma once

/*
 * A bucket-chaining hash table.
 */

#include "spinlock.hh"
#include "lockwrap.hh"
#include "hash.hh"
#include "ilist.hh"
#include "hpet.hh"
#include "cpuid.hh"

/*
 * MV-RLU manages only pure value
 */
namespace mvrlu {
  
  template<class K, class V>
  class chainhash {
  private:
    struct item : public rcu_freed {
      item(const K& k, const V& v)
        : rcu_freed("chainhash::item", this, sizeof(*this)),
          key(k), val(v) {}
      void do_gc() override { delete this; }
      NEW_DELETE_OPS(item);

      islink<item> link;  //ilist.hh -> singly-linked lists
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
    bucket* buckets_;

  public:
    chainhash(u64 nbuckets) : nbuckets_(nbuckets) {
      buckets_ = new bucket[nbuckets_];
      assert(buckets_);
    }

    ~chainhash() {
      delete[] buckets_;
    }

    NEW_DELETE_OPS(chainhash);

    bool insert(const K& k, const V& v, u64 *tsc = NULL) {
      bucket* b = &buckets_[hash(k) % nbuckets_];
      scoped_acquire l(&b->lock);

      for (const item& i: b->chain)
        if (i.key == k)
          return false;

      b->chain.push_front(new item(k, v));
      if (tsc)
        *tsc = get_tsc();
      return true;
    }

    bool remove(const K& k, const V& v, u64 *tsc = NULL) {
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
          gc_delayed(&*i);
          if (tsc)
            *tsc = get_tsc();
          return true;
        }
      }
    }

    bool remove(const K& k, u64 *tsc = NULL) {
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
          gc_delayed(&*i);
          if (tsc)
            *tsc = get_tsc();
          return true;
        }
      }
    }

    bool enumerate(const K* prev, K* out) const {
      scoped_gc_epoch rcu_read;

      bool prevbucket = (prev != nullptr);
      for (u64 i = prev ? hash(*prev) % nbuckets_ : 0; i < nbuckets_; i++) {
        bucket* b = &buckets_[i];
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
      scoped_gc_epoch rcu_read;

      for (u64 i = 0; i < nbuckets_; i++) {
        bucket* b = &buckets_[i];

        for (const item& i: b->chain) {
          if (cb(i.key, i.val))
            return;
        }
      }
    }

    int getSize() {
      scoped_gc_epoch rcu_read;

      int size = 0;

      for (u64 i = 0; i < nbuckets_; i++) {
        bucket* b = &buckets_[i];

        for (const item& i: b->chain) {
          if (i.key != 0)
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
        if (i.key != k)
          continue;
        if (vptr)
          *vptr = i.val;
        return true;
      }
      return false;
    }

  };

}
