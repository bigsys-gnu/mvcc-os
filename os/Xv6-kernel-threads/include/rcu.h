#ifndef RCU_H
#define RCU_H

#define RCU_MAX_FREE_PTRS (1000)
#define MAX_SPIN_LOCKS (10)
#define MAX_THREADS (35)

struct rcu_data {
  /* long *times; */
  long times[MAX_THREADS];
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
  /* rcu_node **rcu_table; */
  rcu_node rcu_table[MAX_THREADS];
};

#endif /* RCU_H */
