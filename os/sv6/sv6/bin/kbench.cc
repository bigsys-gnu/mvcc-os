#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"

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
  exit(2);
}

#define SPINLOCK 0
#define MVRLU 1

int
main(int argc, char **argv)
{
  printf("kernel lavel benchmark start\n");
  int n_buckets = DEFAULT_BUCKETS;
  int initial = DEFAULT_INITIAL;
  int nb_threads = DEFAULT_NB_THREADS;
  int duration = DEFAULT_DURATION;
  int update = DEFAULT_UPDATE;
  int type = SPINLOCK;
  int range = DEFAULT_RANGE;

  int opt;
  while ((opt = getopt(argc, argv, "b:i:t:d:u:s:r:")) != -1)
  {
    switch (opt)
    {
    case 's':
      type = atoi(optarg);
      break;
    case 'u':
      update = atoi(optarg);
      break;
    case 'd':
      duration = atoi(optarg);
      break;
    case 't':
      nb_threads = atoi(optarg);
      break;
    case 'i':
      initial = atoi(optarg);
      break;
    case 'b':
      n_buckets = atoi(optarg);
      break;
    case 'r':
      range = atoi(optarg);
      break;
    default:
      usage(argv[0]);
    }
  }

  assert(n_buckets >= 1);
  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(update >= 0 && update <= 1000);
  // assert(range > 0 && range >= initial);
  // assert(n_buckets < range);

  printf("-t #threads     : %d\n", nb_threads);
  printf("-i Initial size : %d\n", initial);
  printf("-b Buckets      : %d\n", n_buckets);
  printf("-d Duration     : %d\n", duration);
  printf("-u Update rate  : %d\n", update);
  printf("-s sync type    : %d\n", type);
  printf("Benchmark type  : hash-list\n");


  benchmark(
    nb_threads,
    initial,
    n_buckets,
    duration,
    update,
    type);

  return 1;
}
