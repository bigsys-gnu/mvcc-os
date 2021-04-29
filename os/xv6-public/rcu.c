#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "rcu.h"

void rcu_read_lock()
{
  pushcli();
  preempt_disable();
  popcli();
}

void rcu_read_unlock()
{
  pushcli();
  preempt_enable();
  popcli();
}

void call_rcu()
{
    // Schedule this process to run in all the cpus.
    int i = 0;
    while (i < ncpu) {
        cprintf ("\nInside call rcu setting myproc() allowed cpu to %d", i);
        myproc()->allowed_cpu =  i++;
        yield();        
    }
    myproc()->allowed_cpu = -1;
    // You are good to go.
    cprintf ("\n RCU wait done!");
}
