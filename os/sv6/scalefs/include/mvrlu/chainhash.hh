#pragma once

/*
 * A bucket-chaining hash table.
 */

#include "hash.hh"
#include "hpet.hh"
#include "cpuid.hh"
#include "mvrlu/mvrlu.hh"
#include "mvrlu/list.hh"
#include "mvrlu/section.hh"

/*
 * MV-RLU manages only pure value
 */
namespace mvrlu {
  
  template<class K, class V>
  class chainhash {
  private:
    struct item {
      item(const K& k, const V& v)
        : key(k), val(v) {}

      MVRLU_NEW_DELETE(item);

      link<item> link;
      const K key;
      V val;
    };

    struct bucket {
      list<item, &item::link> chain;

      ~bucket() {
        auto &h = myproc()->handle;

        // need fix here
        mvrlu_section s;
        for (auto it = chain.begin(); it != chain.end();) {
          auto trash = it++;
          h.mvrlu_free(&*trash);
        }
        h.mvrlu_free(&*chain.before_begin());
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

    restart:
      mvrlu_section s;
      auto cur = b->chain.before_begin();
      auto prev = cur++;
      for (; ; prev = cur++)
      {
        if (cur == nullptr || cur->key > k)
        {
          if (!prev.try_lock() || !cur.try_lock())
            goto restart;

          b->chain.insert_after(prev, cur, new item(k, v));
          if (tsc)
            *tsc = get_tsc();
          return true;
        }
        else if (cur->key == k)  // duplicated key
          return false;
      }
      return false;
    }

    bool remove(const K& k, const V& v, u64 *tsc = NULL) {
      bucket *b = &buckets_[hash(k) % nbuckets_];

    restart:
      mvrlu_section s;
      auto i = b->chain.before_begin();
      auto end = b->chain.end();
      for (;;)
      {
        auto prev = i++;
        if (i == end)
          return false;
        if (i->key == k && i->val == v)
        {
          if (!prev.try_lock() || !i.try_lock_const())
            goto restart;

          b->chain.erase_after(prev, i);
          myproc()->handle.mvrlu_free(&*i);
          if (tsc)
            *tsc = get_tsc();
          return true;
        }
      }
    }

    bool remove(const K& k, u64 *tsc = NULL) {
      bucket* b = &buckets_[hash(k) % nbuckets_];

    restart:
      mvrlu_section s;
      auto i = b->chain.before_begin();
      auto end = b->chain.end();
      for (;;)
      {
        auto prev = i++;
        if (i == end)
          return false;
        if (i->key == k)
        {
          if (!prev.try_lock() || !i.try_lock_const())
            goto restart;

          b->chain.erase_after(prev, i);
          myproc()->handle.mvrlu_free(&*i);
          if (tsc)
            *tsc = get_tsc();
          return true;
        }
      }
    }

    bool enumerate(const K* prev, K* out) const {
      mvrlu_section s;

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
      mvrlu_section s;

      for (u64 i = 0; i < nbuckets_; i++) {
        bucket* b = &buckets_[i];

        for (const item& i: b->chain) {
          V val = i.val;
          if (cb(i.key, val))
            return;
        }
      }
    }

    int getSize() {
      int size = 0;

      mvrlu_section s;
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
      bucket *b = &buckets_[hash(k) % nbuckets_];
      mvrlu_section s;
      for (const item& i : b->chain)
      {
        if (i.key < k)
          continue;
        else if (i.key > k)
          return false;
        if (vptr)
          *vptr = i.val;
        return true;
      }
      return false;
    }

  };

}
