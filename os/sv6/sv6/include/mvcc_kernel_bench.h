#ifndef MVCC_KERNEL_BENCH_H
#define MVCC_KERNEL_BENCH_H
struct kernel_bench_args {
  int nb_threads;
  int initial;
  int n_buckets;
  int duration;
  int update;
  int range;
  int sync_type;
};

struct kernel_bench_outcome {
  unsigned long total_size;
  unsigned long exp;
  unsigned long total_read;
  unsigned long total_update;
};

#endif /* MVCC_KERNEL_BENCH_H */
