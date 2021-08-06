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

#include "urcu.h"
#include "pthread.h"

#define assert(CONDITION)\
  if (!(CONDITION))	{	 \
	printf(#CONDITION);	 \
    exit(0);          }    \

#define RCU_MAX_FREE_PTRS (1000)
#define MAX_SPIN_LOCKS (10)
#define MAX_THREADS (35)

struct rcu_data {
  long *times;  
  int id;
};

typedef struct rcu_node_t {
    volatile long time; 
	int f_size;
	char p[184];
} rcu_node;

/* rcu_maintain is for rcu maintain thread. */
struct rcu_maintain {
  int threads;
  rcu_node **rcu_table;
};

void rcu_init(struct rcu_maintain *rm, int num_threads);
void rcu_reader_lock(struct rcu_maintain *rm, struct rcu_data *d);
void rcu_reader_unlock(struct rcu_maintain *rm, struct rcu_data *d);
void rcu_synchronize(struct rcu_maintain *rm, struct rcu_data *d);
void rcu_register(struct rcu_maintain *rm, struct rcu_data *d);
void rcu_unregister(struct rcu_data *d);

#endif /* _K_RCU_H_ */
