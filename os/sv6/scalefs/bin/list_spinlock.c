#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define assert(COND)\
  if (!(COND)) {\
  printf(#COND);\
  exit(1);\
  }

#define MAX_BUCKETS (128)
#define DEFAULT_BUCKETS                 1
#define DEFAULT_DURATION                1000
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

typedef struct node {
  int value;
  struct node *next;
} node_t;

typedef struct list {
  node_t *head;
  pthread_mutex_t l;
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
      new_node = (node_t *)malloc(sizeof(node_t));
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
    new_node = (node_t *)malloc(sizeof(node_t));
    new_node->value = key;
    new_node->next = cur;
    prev->next = new_node;
  }
  return ret;
}

int list_insert(int key, list_t *list)
{
  node_t *prev, *cur, *new_node;
  int ret = 0;

  pthread_mutex_lock(&list->l);

  for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
  {
    if (cur->value > key)
    {
      /* initialize node */
      new_node = (node_t *) malloc(sizeof(node_t));
      new_node->value = key;

      /* insert node */
      new_node->next = prev->next;
      prev->next = new_node;
      ret = 1;
      break;
    }
    else if (cur->value == key)
    {
      pthread_mutex_unlock(&list->l);
      return ret;             /* the key value already exists. */
    }    
  }

  if (ret == 0 && cur == NULL)      /* cur is NULL now */
    {
      /* initialize node */
      new_node = (node_t *) malloc(sizeof(node_t));
      new_node->value = key;
      
      /* insert node */
      new_node->next = prev->next;
      prev->next = new_node;
      ret = 1;
    }

  pthread_mutex_unlock(&list->l);
  return ret;
}

int list_delete(int key, list_t *list)
{
  node_t *prev, *cur, *cur_n;
  int ret = 0;

  pthread_mutex_lock(&list->l);
  for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
  {
    /* found the target to be trashed. */
    if (cur->value == key)
    {
      cur_n = cur->next;

      prev->next = cur_n;
      ret = 1;
      break;
    }
  }
  pthread_mutex_unlock(&list->l);
  free(cur);

  return ret;
}

int list_find(int key, list_t *list)
{
  node_t *cur;
  int value = -1;

  pthread_mutex_lock(&list->l);

  cur = list->head;

  while(cur != NULL)
  {
    /* found the value. */
    if (cur->value == key)
	  {
      value = cur->value;
      break;
	  }
	  cur = cur->next;
  }

  pthread_mutex_unlock(&list->l);
  return value;
}

void *test(void* param)
{
  int op, bucket, value;
  value = 1;//sys_uptime();

  thread_param_t *p_data = (thread_param_t*)param; 
  hash_list_t *p_hash_list = p_data->p_hash_list;

  pthread_barrier_wait(&bar);

  while (*p_data->stop == 0)
  {
    op = rand() % 1000;
    value = rand() % (p_data->range);
    bucket = HASH_VALUE(p_hash_list, value);
    list_t *p_list = p_hash_list->buckets[bucket];

    if (op < p_data->update)
    {
      if ((op & 0x01) == 0)
      {
        if (list_insert(value, p_list))
        {
          p_data->variation++;
        }
        p_data->result_add++;
      }
      else
      {
        if (list_delete(value, p_list))
        {
          p_data->variation--;
        }
        p_data->result_remove++;
      }
    }
    else
    {
      if(list_find(value, p_list) >= 0)
      {
        p_data->result_contains++;
      }
      p_data->result_found++;
    }
  }

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
    list->head = (node_t *)malloc(sizeof(node_t));
    list->head->next = NULL;
    p_hash_list->buckets[i] = list;
    /* list->l */
    pthread_mutex_init(&list->l, NULL);
  }

  printf("initialize %d nodes...", initial);
  int j = 0;
  while (j < initial)
    {
      int value = rand() % range;
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
    reads += param_list[i].result_found;
    updates += (param_list[i].result_add + param_list[i].result_remove);
    total_variation += param_list[i].variation;
  }

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
