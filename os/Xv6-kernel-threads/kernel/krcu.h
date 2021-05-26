#ifndef _K_RCU_H_
#define _K_RCU_H_

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

#include "rcu.h"
#include "types.h"
#include "defs.h"
#include "spinlock.h"

#define assert(CONDITION)\
  if (!(CONDITION))		 \
	panic(#CONDITION)	 \

#define MAX_SPIN_LOCKS (10)

typedef struct rcu_node_t {
    volatile long time; 
	int f_size;
	void *free_ptrs[RCU_MAX_FREE_PTRS];
	char p[184];
} rcu_node;

/* rcu_maintain is for rcu maintain thread. */
struct rcu_maintain {
  int threads;
  rcu_node **rcu_table;
  struct spinlock urcu_spin[MAX_SPIN_LOCKS];
};

#endif /* _K_RCU_H_ */
