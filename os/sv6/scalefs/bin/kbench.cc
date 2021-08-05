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
#define DEFAULT_DURATION                15000
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              4
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)


int
main(int argc, char **argv)
{
    printf("kernel lavel benchmark start\n");
	int n_buckets = DEFAULT_BUCKETS;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	int duration = DEFAULT_DURATION;
	int update = DEFAULT_UPDATE;
	int range = DEFAULT_RANGE;

    switch (argc - 1)
    {
    case 6:
        range = atoi(argv[6]);
    case 5:
        update = atoi(argv[5]);
    case 4:
        duration = atoi(argv[4]);
    case 3:
        n_buckets = atoi(argv[3]);
    case 2:
        initial = atoi(argv[2]);
    case 1:
        nb_threads = atoi(argv[1]);
    default:
        printf("%d Option is inserted\n", argc - 1);
        break;
    }
    printf("-Nb threads   : %d\n", nb_threads);
    printf("-Initial size : %d\n", initial);
    printf("-Buckets      : %d\n", n_buckets);
    printf("-Duration     : %d\n", duration);
    printf("-Update rate  : %d\n", update);
    printf("-range        : %d\n", range);
    printf("-Set type     : hash-list\n");

	assert(n_buckets >= 1);
	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(update >= 0 && update <= 1000);
	assert(range > 0 && range >= initial);


    benchmark(
        nb_threads,
        initial,
        n_buckets,
        duration,
        update,
        range);

    return 1;
}