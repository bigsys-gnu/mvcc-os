#ifndef RCU_H
#define RCU_H

#define RCU_MAX_FREE_PTRS (1000)

struct rcu_data {
  long *times;
  int id;
};

#endif /* RCU_H */
