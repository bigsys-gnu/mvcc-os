#include "types.h"
#include "user.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "x86.h"

#undef NULL
#define NULL ((void*)0)

#define PGSIZE (4096)

#define MAX_BUCKETS (2000)
#define DEFAULT_BUCKETS                 1
#define DEFAULT_DURATION                100
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

int main(int argc, char **argv)
{
    int rtval = 0;
    rtval = bench();
    printf(1, "rtval: %d\n", rtval);
    exit();
}
