#ifndef PORT_DATA_H
#define PORT_DATA_H
/*
 * Data Types
 */

struct task_struct {
  int val;
};

typedef struct {
  void *spin_obj;
} spinlock_t;

struct mutex {
  void *mutex_obj;
};

struct completion {
  void *cond_obj;
};

#endif /* PORT_DATA_H */
