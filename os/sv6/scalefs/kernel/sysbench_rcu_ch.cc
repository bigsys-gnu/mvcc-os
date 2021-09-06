#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "mmu.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "vm.hh"
#include "kmtrace.hh"
#include "futex.h"
#include "version.hh"
#include "filetable.hh"

#include <uk/mman.h>
#include <uk/utsname.h>
#include <uk/unistd.h>

#define MAX_BUCKETS (128)
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)


//SYSCALL
void
sys_benchmark_rcu_ch(int th, int init, int buck, int dur, int upd, int rng)
{
    cprintf("Run Kernel Level Benchmark for RCU chainhash\n");


    cprintf("Kernel Level Benchmark for RCU chainhash END\n");
}
