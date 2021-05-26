#include "krcu.h"
#include "mmu.h"
#include "param.h"
#include "x86.h"
#include "syscall.h"
#include "proc.h"
#include "malloc.h"


/**
 * Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).
 * 
 * This file is part of Citrus. 
 * 
 * Citrus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Authors Maya Arbel and Adam Morrison 
 */

void rcu_init(struct rcu_maintain *rm, int num_threads)
{
  rcu_node **result = (rcu_node**) malloc(sizeof(rcu_node)*num_threads);
  int i;
  rcu_node *new;

  rm->threads = num_threads; 
  for (i = 0; i < rm->threads; i++)
	{
	  new = (rcu_node*) malloc(sizeof(rcu_node));
	  new->time = 1; 
	  new->f_size = 0;
	  *(result + i) = new;
	}

  rm->rcu_table =  result;

  /* need to be fixed. */
  for (i = 0; i < MAX_SPIN_LOCKS; i++)
	initlock(&(rm->urcu_spin[i]), "rcu");

  return; 
}

void rcu_register(struct rcu_maintain *rm, struct rcu_data *d)
{
  d->times = (long*) malloc(sizeof(long)* (rm->threads));
  d->id = proc->pid;
  if (d->times == NULL )
	panic("malloc failed");
}

void rcu_unregister(struct rcu_data *d)
{
  free(d->times);
}

void rcu_reader_lock(struct rcu_maintain *rm, struct rcu_data *d)
{
  assert(rm->rcu_table[d->id] != NULL);
  __sync_add_and_fetch(&rm->rcu_table[d->id]->time, 1);
}

static inline void set_bit(int nr, volatile unsigned long *addr)
{
  asm("btsl %1,%0" : "+m" (*addr) : "Ir" (nr));
}

void rcu_reader_unlock(struct rcu_maintain *rm, struct rcu_data *d)
{
  assert(rm->rcu_table[d->id]!= NULL);
  set_bit(0, (unsigned long *)&rm->rcu_table[d->id]->time);
}

/* need to be fixed */
void rcu_writer_lock(struct rcu_maintain *rm, int lock_id)
{
  assert(lock_id < MAX_SPIN_LOCKS);
  acquire(&(rm->urcu_spin[lock_id]));
}

/* need to be fixed */
void rcu_writer_unlock(struct rcu_maintain *rm, int lock_id)
{
  assert(lock_id < MAX_SPIN_LOCKS);
  release(&(rm->urcu_spin[lock_id]));
}

void rcu_synchronize(struct rcu_maintain *rm, struct rcu_data *d)
{
  int i; 
  //read old counters
  for(i = 0; i < rm->threads; i++)
	d->times[i] = rm->rcu_table[i]->time;
  for(i = 0; i < rm->threads; i++)
	{
	  if (d->times[i] & 1) continue;
	  while (1)
		{
		  unsigned long t = rm->rcu_table[i]->time;
		  if (t & 1 || t > d->times[i])
			{
			  break; 
			}
		}
	}
}

void rcu_free(struct rcu_maintain *rm, struct rcu_data *d, void *ptr)
{
  int k;
	
  rm->rcu_table[d->id]->free_ptrs[rm->rcu_table[d->id]->f_size] = ptr;
  rm->rcu_table[d->id]->f_size++;
  if (rm->rcu_table[d->id]->f_size == RCU_MAX_FREE_PTRS)
	{
	  rcu_synchronize(rm, d);
		
	  for (k = 0; k < rm->rcu_table[d->id]->f_size; k++)
		free(rm->rcu_table[d->id]->free_ptrs[k]);

	  rm->rcu_table[d->id]->f_size = 0;
	}
}
