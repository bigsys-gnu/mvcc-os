#include "new-urcu.h"
#include "types.h"
#include "user.h"


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

int threads; 
rcu_node** urcu_table;
int ppid;

#define MAX_SPIN_LOCKS (10000)
/*typedef struct _my_lock {
	pthread_spinlock_t lock;
	//long padding[4];	
} my_lock;*/

#define assert(x) if (x) {} else { \
   printf(1, "%s: %d ", __FILE__, __LINE__); \
   printf(1, "assert failed (%s)\n", # x); \
   printf(1, "TEST FAILED\n"); \
   kill(ppid); \
   exit(); \
}


lock_t urcu_spin[MAX_SPIN_LOCKS];

void urcu_init(int num_threads){
    
   ppid = getpid();
   rcu_node** result = (rcu_node**) malloc(sizeof(rcu_node)*num_threads);
   int i;
   rcu_node* new;
   threads = num_threads; 
   for( i=0; i<threads ; i++){
        new = (rcu_node*) malloc(sizeof(rcu_node));
        new->time = 1; 
		new->f_size = 0;
        *(result + i) = new;
    }

    urcu_table =  result;

	for (i = 0; i < MAX_SPIN_LOCKS; i++) {
		lock_init(&(urcu_spin[i]));
	}	

    printf(1, "initializing URCU finished, node_size: %d\n", sizeof(rcu_node));
    return; 
}

/* __thread long* times = NULL;  */
/* __thread int i; */

void urcu_register(struct urcu_data *ud){
    printf(1, "test");
    ud->times = (long*) malloc(sizeof(long)*threads);
    ud->id = getpid(); 
    if (ud->times == NULL ){
        printf(1, "malloc failed\n");
        exit();
    }
}
void urcu_unregister(struct urcu_data *ud){
    free(ud->times);
}

void urcu_reader_lock(struct urcu_data *ud){
    assert(urcu_table[ud->id] != NULL);
    __sync_add_and_fetch(&urcu_table[ud->id]->time, 1);
}


static inline void set_bit(int nr, volatile unsigned long *addr){
    asm("btsl %1,%0" : "+m" (*addr) : "Ir" (nr));
}

void urcu_reader_unlock(struct urcu_data *ud){
    assert(urcu_table[ud->id]!= NULL);
    set_bit(0, (unsigned long *)&urcu_table[ud->id]->time);
}

void urcu_writer_lock(int lock_id){
	lock_acquire(&(urcu_spin[lock_id]));
}

void urcu_writer_unlock(int lock_id){
	lock_release(&(urcu_spin[lock_id]));
}

void urcu_synchronize(struct urcu_data *ud){
    int i; 
    //read old counters
    for( i=0; i<threads ; i++){
        ud->times[i] = urcu_table[i]->time;
    }
    for( i=0; i<threads ; i++){
        if (ud->times[i] & 1) continue;
        while(1){
            unsigned long t = urcu_table[i]->time;
            if (t & 1 || t > ud->times[i]){
                break; 
            }
        }
    }
}

void urcu_free(struct urcu_data *ud, void *ptr) {
	int k;
	
	urcu_table[ud->id]->free_ptrs[urcu_table[ud->id]->f_size] = ptr;
	urcu_table[ud->id]->f_size++;
	
	if (urcu_table[ud->id]->f_size == URCU_MAX_FREE_PTRS) {
		
		urcu_synchronize(ud);
		
		for (k = 0; k < urcu_table[ud->id]->f_size; k++) {
			free(urcu_table[ud->id]->free_ptrs[k]);
		}
		
		urcu_table[ud->id]->f_size = 0;
	}
}
