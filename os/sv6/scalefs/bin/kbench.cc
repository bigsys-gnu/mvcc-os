// SPDX-FileCopyrightText: Copyright (c) 2021 Gyeongsang National University
//
// SPDX-License-Identifier: Apache 2.0 AND MIT

#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"
#include "mvcc_kernel_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define MAX_BUCKETS (128)
#define DEFAULT_BUCKETS                 2
#define DEFAULT_DURATION                1500
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              4
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)
#define PRINT_BENCH(sync_type) printf("************ %s ***************\n", \
                                      type_names[sync_type])

void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s [options] nthreads\n", argv0);
  fprintf(stderr, "  -b the number of buckets\n");
  fprintf(stderr, "  -i the number of initial nodes\n");
  fprintf(stderr, "  -t the number of threads\n");
  fprintf(stderr, "  -d micro seconds of benchmark\n");
  fprintf(stderr, "  -u update ratio (20 is 2%%)\n");
  fprintf(stderr, "  -s sync type\n");
  fprintf(stderr, "  -r range\n");
  exit(2);
}

enum {
  SPINLOCK, MVRLU, RCU, SPIN_CHAIN
};

const char *type_names[] = {
  "SPINLOCK",
  "MVRLU",
  "RCU",
  "SPIN CHAIN",
};

int
main(int argc, char **argv)
{
  kernel_bench_args arguments = {
    .nb_threads = DEFAULT_NB_THREADS,
    .initial = DEFAULT_INITIAL,
    .n_buckets = DEFAULT_BUCKETS,
    .duration = DEFAULT_DURATION,
    .update = DEFAULT_UPDATE,
    .range = DEFAULT_RANGE,
    .sync_type = SPINLOCK,
  };
  kernel_bench_outcome outcome;

  printf("kernel lavel benchmark start\n");

  int opt;
  while ((opt = getopt(argc, argv, "b:i:t:d:u:s:r:")) != -1)
  {
    switch (opt)
    {
    case 's':
      arguments.sync_type = atoi(optarg);
      break;
    case 'u':
      arguments.update = atoi(optarg);
      break;
    case 'd':
      arguments.duration = atoi(optarg);
      break;
    case 't':
      arguments.nb_threads = atoi(optarg);
      break;
    case 'i':
      arguments.initial = atoi(optarg);
      break;
    case 'b':
      arguments.n_buckets = atoi(optarg);
      break;
    case 'r':
      arguments.range = atoi(optarg);
      break;
    default:
      usage(argv[0]);
    }
  }

  assert(arguments.n_buckets >= 1);
  assert(arguments.duration >= 0);
  assert(arguments.initial >= 0);
  assert(arguments.nb_threads > 0);
  assert(arguments.update >= 0 && arguments.update <= 1000);
  assert(arguments.range > 0 && arguments.range >= arguments.initial);
  assert(arguments.n_buckets < arguments.range);

  printf("-t #threads     : %d\n", arguments.nb_threads);
  printf("-i Initial size : %d\n", arguments.initial);
  printf("-b Buckets      : %d\n", arguments.n_buckets);
  printf("-d Duration     : %d\n", arguments.duration);
  printf("-u Update rate  : %d\n", arguments.update);
  printf("-s sync type    : %d\n", arguments.sync_type);
  printf("-r Range        : %d\n", arguments.range);
  printf("Benchmark type  : hash-list\n");


  benchmark((void *)&arguments, (void *)&outcome);

  printf( "\n#### B ####\n");

  printf( "Set size      : %lu (expected: %lu)\n", outcome.total_size, outcome.exp);
  printf( "Duration      : %d (ms)\n", arguments.duration);
  unsigned long iv = outcome.total_read * 1000.0 / arguments.duration;
  unsigned long fv = (unsigned long)(outcome.total_read * 1000.0 / arguments.duration * 10) % 10;
  printf( "#read ops     : %lu (%lu.%lu / s)\n", outcome.total_read, iv, fv);
  iv = outcome.total_update * 1000.0 / arguments.duration;
  fv = (unsigned long)(outcome.total_update * 1000.0 / arguments.duration * 10) % 10;
  printf( "#update ops   : %lu (%lu.%lu / s)\n", outcome.total_update, iv, fv);

  if(outcome.exp != outcome.total_size)
  {
    printf("\n<<<<<< ASSERT FAILURE(%lu!=%lu) <<<<<<<<\n", outcome.exp,
           outcome.total_size);
  }
  PRINT_BENCH(arguments.sync_type);

  return 1;
}
