// SPDX-FileCopyrightText: Copyright (c) 2021 Gyeongsang National University
//
// SPDX-License-Identifier: Apache 2.0 AND MIT

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "user.h"
#include "mvrlu/mvrlu.h"
#include "mvrlu/mvrlu_i.h"

#define MAX_BUCKETS (2048)
#define DEFAULT_BUCKETS                 1
#define DEFAULT_DURATION                1000
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)
/////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////
static inline int MarsagliaXORV (int x) { 
  if (x == 0) x = 1 ; 
  x ^= x << 6;
  x ^= ((unsigned)x) >> 21;
  x ^= x << 7 ; 
  return x ;        // use either x or x & 0x7FFFFFFF
}

static inline int MarsagliaXOR (int * seed) {
  int x = MarsagliaXORV(*seed);
  *seed = x ; 
  return x & 0x7FFFFFFF;
}

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  
  /*int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);*/
  
  int v = MarsagliaXOR((int *)seed) % n;
  return v;
}

typedef struct node {
  int value;
  struct node *next;
} node_t;

typedef struct list {
  node_t *head;
} list_t;

typedef struct hash_list {
  int n_buckets;
  list_t *buckets[MAX_BUCKETS];  
} hash_list_t;

typedef struct thread_param {
  int n_buckets;
  int id;
  int nb_threads;
  int update;
  int range;
  int variation;
  int result_add;
  int result_remove;
  int result_contains;
  int result_found;
  int *stop;
  hash_list_t *p_hash_list;
  rlu_thread_data_t self; // for MV-RLU
  unsigned short seed[3]; // for MV-RLU
} thread_param_t;

static pthread_barrier_t bar;
int n_buckets = DEFAULT_BUCKETS;
int initial = DEFAULT_INITIAL;
int nb_threads = DEFAULT_NB_THREADS;
int duration = DEFAULT_DURATION;
int update = DEFAULT_UPDATE;
int range = DEFAULT_RANGE;

int
raw_list_insert(int key, list_t *list)
{
  node_t *prev, *cur, *new_node;
  int ret = 0;

  for (prev = list->head, cur = prev->next; cur != NULL;
       prev = cur, cur = cur->next)
  {
    if (key < cur->value)
    {
      ret = 1;
      new_node = RLU_ALLOC(sizeof(node_t));
      new_node->value = key;
      new_node->next = cur;
      prev->next = new_node;
      return ret;
    }
    else if (key == cur->value)
      return ret;             /* already exists */
  }

  if (cur == NULL)
    {
      ret = 1;
      new_node = RLU_ALLOC(sizeof(node_t));
      new_node->value = key;
      new_node->next = cur;
      prev->next = new_node;
    }
  return ret;
}

int list_insert(rlu_thread_data_t *self, int key, list_t *list)
{
  node_t *prev, *cur, *new_node;
  int ret = 0;

 restart:
  RLU_READER_LOCK(self);

  prev = (node_t *)RLU_DEREF(self, list->head); 
  cur = (node_t *)RLU_DEREF(self, prev->next);

  for (;; prev = cur, cur = (node_t *)RLU_DEREF(self, cur->next))
    {
      if (cur == NULL || cur->value > key)
        {
          /* get the lock */
          if (!RLU_TRY_LOCK(self, &prev))
            {
              RLU_ABORT(self);
              goto restart;
            }
          if (cur && !RLU_TRY_LOCK(self, &cur))
            {
              RLU_ABORT(self);
              goto restart;
            }
          /* initialize node */
          new_node = (node_t *) RLU_ALLOC(sizeof(node_t));
          new_node->value = key;

          /* insert node */
          RLU_ASSIGN_PTR(self, &new_node->next, cur);
          RLU_ASSIGN_PTR(self, &prev->next, new_node);
          ret = 1;
		      break;
        }
      else if (cur->value == key)
      {
        break;             /* the key value already exists. */
      }
    }

  RLU_READER_UNLOCK(self);
  return ret;
}

int list_delete(rlu_thread_data_t *self, int key, list_t *list)
{
  node_t *prev, *cur, *cur_n;
  int ret = 0;

 restart:
  RLU_READER_LOCK(self);

  prev = (node_t *)RLU_DEREF(self, list->head);
  cur = (node_t *)RLU_DEREF(self, prev->next);

  for ( ; cur != NULL; 
         prev = cur, cur = (node_t *)RLU_DEREF(self, cur->next))
    {
      /* found the target to be trashed. */
      if (cur->value == key)
        {
          /* try lock */
          if (!RLU_TRY_LOCK(self, &prev) ||
			  !RLU_TRY_LOCK_CONST(self, cur))
            {
              RLU_ABORT(self);
              goto restart;
            }
		  cur_n = (node_t *)RLU_DEREF(self, cur->next);
          RLU_ASSIGN_PTR(self, &prev->next, cur_n);
          RLU_FREE(self, cur);
          ret = 1;
          break;
        }
    }

  RLU_READER_UNLOCK(self);
  return ret;
}

int list_find(rlu_thread_data_t *self, int key, list_t *list)
{
  node_t *cur;
  int value = -1;

  RLU_READER_LOCK(self);

  cur = (node_t *)RLU_DEREF(self, list->head);

  while(cur && cur->value < key)
  {
	cur = (node_t *)RLU_DEREF(self, cur->next);
  }

  /* found the value. */
  if (cur && cur->value == key)
	{
	  value = cur->value;
	}

  RLU_READER_UNLOCK(self);
  return value;
}

void *test(void* param)
{
  int op, bucket, value;
  value = 1;//sys_uptime();

  thread_param_t *p_data = (thread_param_t*)param; 
  hash_list_t *p_hash_list = p_data->p_hash_list;
  rlu_thread_data_t *self = &p_data->self;

  if (setaffinity((p_data->id + 1) % NCPU) < 0)
  {
    RLU_THREAD_FINISH(self);
    RLU_THREAD_FREE(self);
    die("affinity error");
  }
  mvrlu_flush_log(self);
  pthread_barrier_wait(&bar);

  while (*p_data->stop == 0)
    {
      op = rand_range(1000, p_data->seed);
      value = rand_range(p_data->range, p_data->seed);
      bucket = HASH_VALUE(p_hash_list, value);
      list_t *p_list = p_hash_list->buckets[bucket];

      if (op < p_data->update)
        {
          if ((op & 0x01) == 0)
            {
              if (list_insert(self, value, p_list))
                {
                  p_data->variation++;
                }
              p_data->result_add++;
            }
          else
            {
              if (list_delete(self, value, p_list))
                {
                  p_data->variation--;
                }
              p_data->result_remove++;
            }
        }
      else
        {
          if(list_find(self, value, p_list) >= 0)
            {
              p_data->result_found++;
            }
          p_data->result_contains++;
        }
    }

  RLU_THREAD_FINISH(self);
  RLU_THREAD_FREE(self);
  printf("thread %d end\n", p_data->id);
  return NULL;
}

int main(int argc, char **argv)
{
  thread_param_t *param_list;
  hash_list_t *p_hash_list;
  pthread_t *thread_list;
  int stop = 0;
  unsigned long exp = 0, total_variation = 0, total_size = 0;
  unsigned long reads = 0, updates = 0;
  unsigned long iv = 0, fv = 0;

  int i;

  RLU_INIT();

  srand(time(NULL));

  switch (argc - 1)
    {
    case 6:
      range = atoi(argv[6]);
    case 5:
      update = atoi(argv[5]);
    case 4:
      duration = atoi(argv[4]);
    case 3:
      n_buckets = atoi(argv[3]);
    case 2:
      initial = atoi(argv[2]);
    case 1:
      nb_threads = atoi(argv[1]);
    default:
      printf("%d Option is inserted\n", argc - 1);
      break;
    }
  printf("\n#### mvcc bench ####\n");
  printf( "-Nb threads   : %d\n", nb_threads);
  printf( "-Initial size : %d\n", initial);
  printf( "-Buckets      : %d\n", n_buckets);
  printf( "-Duration     : %d\n", duration);
  printf( "-Update rate  : %d\n", update);
  printf( "-range        : %d\n", range);
  printf( "-Set type     : hash-list\n");

  assert(n_buckets >= 1 && n_buckets <= MAX_BUCKETS);
  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(update >= 0 && update <= 1000);
  assert(range > 0 && range >= initial);

  p_hash_list = (hash_list_t *)malloc(sizeof(hash_list_t));
  if (p_hash_list == NULL) {
    printf("hash_list init error\n");
    exit(1);
  }
  p_hash_list->n_buckets = n_buckets;

  for (i = 0; i < p_hash_list->n_buckets; i++) {
    list_t *list;
    list = (list_t *)malloc(sizeof(list_t));
    if (list == NULL) {
      printf("list init error\n");
      exit(1);
    }
    list->head = RLU_ALLOC(sizeof(node_t));
    list->head->next = NULL;
    p_hash_list->buckets[i] = list;
  }

  printf("initialize %d nodes...", initial);
  int j = 0;
  unsigned short seed[3];
  rand_init(seed);
  while (j < initial)
    {
      int value = rand_range(range, seed);
      int bucket = HASH_VALUE(p_hash_list, value);

      if (raw_list_insert(value, p_hash_list->buckets[bucket]))
        {
          j++;
        }
    }
  printf("done\n");

  thread_list = (pthread_t *)malloc(nb_threads*sizeof(pthread_t));
  if (thread_list == NULL) {
    printf("thread_list init error\n");
    return 1;
  }

  param_list = (thread_param_t *)malloc(nb_threads*sizeof(thread_param_t));
  if (param_list == NULL) {
    printf("param_list init error\n");
    return 1;
  }

  pthread_barrier_init(&bar, 0, nb_threads);
   
  printf("Creating %d threads...", nb_threads);
  for(i = 0; i < nb_threads; i++)
    {
      param_list[i].n_buckets = n_buckets;
      param_list[i].id = i;
      param_list[i].nb_threads = nb_threads;
      param_list[i].update = update;
      param_list[i].range = range;
      param_list[i].stop = &stop;
      param_list[i].variation = 0;
      param_list[i].p_hash_list = p_hash_list;
	  RLU_THREAD_INIT(&param_list[i].self);
	  rand_init(param_list[i].seed);

      pthread_create(&thread_list[i], 0, test, (void*)&param_list[i]);
    }
  printf(" done!\n");

  sleep(duration / 1000);
  stop = 1;

  printf("join %d threads...\n", nb_threads);
  for(i = 0; i < nb_threads; i++)
    {
      pthread_join(thread_list[i], 0);
    }
  printf(" done!\n");

  printf("\n####result####\n");
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %d\n", param_list[i].result_add);
    printf("  #remove     : %d\n", param_list[i].result_remove);
    printf("  #contains   : %d\n", param_list[i].result_contains);
    printf("  #found      : %d\n", param_list[i].result_found);
    reads += param_list[i].result_contains;
    updates += (param_list[i].result_add + param_list[i].result_remove);
    total_variation += param_list[i].variation;
  }

  RLU_FINISH();

  RLU_PRINT_STATS();

  total_size = 0;
  for(i = 0; i < n_buckets; i++)
    {
      node_t *node = p_hash_list->buckets[i]->head->next;
      while(node != NULL)
        {
          node = node->next;
          total_size++;
        }
    }
  exp = initial + total_variation;

  printf("\n#### total ####\n");
  printf("Set size      : %lu (expected: %lu)\n", total_size, exp);
  printf("Duration      : %d (ms)\n", duration);
  iv = (reads + updates) * 1000.0 / duration;
  fv = (int)((reads + updates) * 1000.0 / duration * 10) % 10;
  printf("#ops          : %lu (%lu.%lu / s)\n", reads + updates, iv, fv);
  iv = reads * 1000.0 / duration;
  fv = (int)(reads * 1000.0 / duration * 10) % 10;
  printf("#read ops     : %lu (%lu.%lu / s)\n", reads, iv, fv);
  iv = updates * 1000.0 / duration;
  fv = (int)(updates * 1000.0 / duration * 10) % 10;
  printf("#update ops   : %lu (%lu.%lu / s)\n", updates, iv, fv);

  if(exp != total_size)
    {
      printf("\n<<<<<< ASSERT FAILURE(%ld!=%ld) <<<<<<<<\n", exp, total_size);
    }
  printf("benchlist end\n");

  return 0;
}
