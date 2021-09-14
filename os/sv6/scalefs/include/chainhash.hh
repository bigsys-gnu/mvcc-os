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
  chainhash(u64 nbuckets) : nbuckets_(nbuckets), dead_(false) {
    buckets_ = new bucket[nbuckets_];
    assert(buckets_);
  }

  ~chainhash() {
    delete[] buckets_;
  }

  NEW_DELETE_OPS(chainhash);

  bool insert(const K& k, const V& v, u64 *tsc = NULL) {
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

  bool replace_from(const K& kdst, const V* vpdst, chainhash* src,
                    const K& ksrc, const V& vsrc, chainhash *subdir,
                    const K& ksubdir, const V& vsubdir, u64 *tsc = NULL)
  {
    /*
     * A special API used by rename.  Atomically performs the following
     * steps, returning false if any of the checks fail:
     *
     *  For file renames:
     *  - checks that this hash table has not been killed (by unlink)
     *  - if vpdst!=nullptr, checks this[kdst]==*vpdst
     *  - if vpdst==nullptr, checks this[kdst] is not set
     *  - checks src[ksrc]==vsrc
     *  - removes src[ksrc]
     *  - sets this[kdst] = vsrc
     *
     * For directory renames (i.e., subdir != nullptr), in addition to the
     * above:
     *  - checks that subdir's hash table has not been killed (by unlink)
     *  - sets subdir[ksubdir] = vsubdir
     *  - TODO: Also deal with the directory that was replaced from the
     *          destination directory by subdir.
     */
    bucket* bdst = &buckets_[hash(kdst) % nbuckets_];
    bucket* bsrc = &src->buckets_[hash(ksrc) % src->nbuckets_];
    bucket* bsubdir = subdir ?
                     (&subdir->buckets_[hash(ksubdir) % subdir->nbuckets_]) :
                     nullptr;

    // Acquire the locks for the source, destination and the subdir directory
    // hash tables in the order of increasing bucket addresses.
    scoped_acquire lk[3];
    std::vector<bucket*> buckets;

    if (bsubdir != nullptr && bsubdir != bsrc && bsubdir != bdst)
      buckets.push_back(bsubdir);
    if (bsrc != bdst)
      buckets.push_back(bsrc);
    buckets.push_back(bdst);
    std::sort(buckets.begin(), buckets.end());

    int i = 0;
    for (auto &b : buckets)
      lk[i++] = b->lock.guard();

    /*
     * Abort the rename if the destination directory's hash table has been
     * killed by a concurrent unlink.
     */
    if (killed())
      return false;

    if (subdir && subdir->killed())
      return false;

    auto srci = bsrc->chain.before_begin();
    auto srcend = bsrc->chain.end();
    auto srcprev = srci;
    for (;;) {
      ++srci;
      if (srci == srcend)
        return false;
      if (srci->key != ksrc) {
        srcprev = srci;
        continue;
      }
      if (srci->val != vsrc)
        return false;
      break;
    }

    for (item& i: bdst->chain) {
      if (i.key == kdst) {
        if (vpdst == nullptr || i.val != *vpdst)
          return false;
        auto w = i.seq.write_begin();
        i.val = vsrc;
        bsrc->chain.erase_after(srcprev);
        gc_delayed(&*srci);

        if (bsubdir != nullptr) {
          for (item& isubdir : bsubdir->chain) {
            if (isubdir.key == ksubdir) {
              auto wsubdir = isubdir.seq.write_begin();
              isubdir.val = vsubdir;
            }
          }
        }

        if (tsc)
          *tsc = get_tsc();
        return true;
      }
    }

    if (vpdst != nullptr)
      return false;

    bsrc->chain.erase_after(srcprev);
    gc_delayed(&*srci);
    bdst->chain.push_front(new item(kdst, vsrc));

    if (bsubdir != nullptr) {
      for (item& isubdir : bsubdir->chain) {
        if (isubdir.key == ksubdir) {
          auto wsubdir = isubdir.seq.write_begin();
          isubdir.val = vsubdir;
        }
      }
    }

    if (tsc)
      *tsc = get_tsc();
    return true;
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
        V val = *seq_reader<V>(&i.val, &i.seq);
        if (cb(i.key, val))
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
        *vptr = *seq_reader<V>(&i.val, &i.seq);
      return true;
    }
    return false;
  }

  bool remove_and_kill(const K& k, const V& v) {
    if (dead_)
      return false;

    for (u64 i = 0; i < nbuckets_; i++)
      for (const item& ii: buckets_[i].chain)
        if (ii.key != k || ii.val != v)
          return false;

    for (u64 i = 0; i < nbuckets_; i++)
      buckets_[i].lock.acquire();

    bool killed = !dead_;
    for (u64 i = 0; i < nbuckets_; i++)
      for (const item& ii: buckets_[i].chain)
        if (ii.key != k || ii.val != v)
          killed = false;

    if (killed) {
      dead_ = true;
      bucket* b = &buckets_[hash(k) % nbuckets_];
      item* i = &b->chain.front();
      assert(i->key == k && i->val == v);
      b->chain.pop_front();
      gc_delayed(i);
    }

    for (u64 i = 0; i < nbuckets_; i++)
      buckets_[i].lock.release();

    return killed;
  }

  bool killed() const {
    return dead_;
  }
};
