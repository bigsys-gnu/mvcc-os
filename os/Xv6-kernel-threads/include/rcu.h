#ifndef RCU_H
#define RCU_H

#define RCU_MAX_FREE_PTRS (1000)

typedef struct rcu_node_t {
    volatile long time; 
	int f_size;
	void *free_ptrs[RCU_MAX_FREE_PTRS];
	char p[184];
} rcu_node;

struct rcu_data {
  long *times;
  int id;
};

#endif /* RCU_H */
