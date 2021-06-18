# Benchmarks for Synchronization Techniques

by evaluating hash table performance on Xv6.
the benchmarks are distributed separately by each synchronization technique like:
- benchlist
- benchlist_spinlock
- benchlist_rcu
- ...

## List of Synchronization Techniques

- Spinlock
- RCU
- RLU (TBR)
- MVRLU (TBR)

## Basic Usage

1. run benchmark on Xv6.
> $ benchlist
2. or run it with parameters.
> $ benchlist (#thread) (#initial node) (bucket size) (time duration, ms) (update ratio) (range of key value)

Caution: order of the parameters is fixed. but you can omit it's backside in order.

## Dependency

- thread
- spinlock, rcu, rlu, ...