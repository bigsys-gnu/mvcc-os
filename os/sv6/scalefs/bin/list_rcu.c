#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "urcu.h"

#undef NULL
#define NULL ((void*)0)

#define PGSIZE (4096)

#define MAX_BUCKETS (128)
#define DEFAULT_BUCKETS                 1
#define DEFAULT_DURATION                1000
#define DEFAULT_UPDATE                  200
#define DEFAULT_INITIAL                 32
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

struct rcu_maintain rcu_global;
static pthread_barrier_t bar;

typedef struct node {
    int value;
    struct node *next;
} node_t;

typedef struct spinlock_list {
    int bucket_id;
    pthread_mutex_t lk;
    node_t *head;
} spinlock_list_t;

typedef struct hash_list {
	int n_buckets;
	spinlock_list_t *buckets[MAX_BUCKETS];  
} hash_list_t;

typedef struct thread_param {
	int n_buckets;
	int initial;
	int nb_threads;
    int update;
    int range;
    unsigned long variation;
    unsigned long result_add;
    unsigned long result_remove;
    unsigned long result_contains;
    unsigned long result_found;
    int *stop;
    hash_list_t *p_hash_list;
    int tidx;
} thread_param_t;

int list_deletes()
{
   return 0; 
}

int random(void)
{
  // Take from http://stackoverflow.com/questions/1167253/implementation-of-rand
  static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
  unsigned int b;
  b  = ((z1 << 6) ^ z1) >> 13;
  z1 = ((z1 & 4294967294U) << 18) ^ b;
  b  = ((z2 << 2) ^ z2) >> 27; 
  z2 = ((z2 & 4294967288U) << 2) ^ b;
  b  = ((z3 << 13) ^ z3) >> 21;
  z3 = ((z3 & 4294967280U) << 7) ^ b;
  b  = ((z4 << 3) ^ z4) >> 12;
  z4 = ((z4 & 4294967168U) << 13) ^ b;

  return (z1 ^ z2 ^ z3 ^ z4) / 2;
}

// Return a random integer between a given range.
int randomrange(int lo, int hi)
{
  if (hi < lo) {
    int tmp = lo;
    lo = hi;
    hi = tmp;
  }
  int range = hi - lo + 1;
  return random() % (range) + lo;
}

int list_insert(int key, spinlock_list_t *list)
{
    node_t *prev, *cur, *new_node;
    int ret=1;

	new_node = (node_t*)malloc(sizeof(node_t));

	pthread_mutex_lock(&list->lk);

    if(list->head == NULL)
    {
        new_node->next = NULL;
        new_node->value = key;
        list->head = new_node;
		pthread_mutex_unlock(&list->lk);
		return ret;
    }
    else
    {
        prev = list->head;
        cur = prev->next;
        for(; cur!=NULL; prev=cur, cur=cur->next)
        {
            if(cur->value == key){
                ret = 0;
                break;
            }
            // sleep(0);
        }
    }

    if(ret){
        //no node with key value
        new_node->next = NULL;
        new_node->value = key;
		do {prev->next = new_node;
		  __sync_synchronize();
		} while(0);
		
    }
    else{
	  free(new_node);
    }
    pthread_mutex_unlock(&list->lk);
    return ret;
}

int list_delete(int key, spinlock_list_t *list, struct rcu_data *d)
{
    node_t *prev, *cur;
    int ret = 0;

	pthread_mutex_lock(&list->lk);

    if(list->head == NULL){
        pthread_mutex_unlock(&list->lk);
        return ret;
    }
    else{
        prev = list->head;
        cur = prev->next;
        for(; cur!=NULL; prev=cur, cur=cur->next)
        {
            if(cur->value == key){
                ret = 1;
                break;
            }
        }
    }

    if(ret)
    {
        //node to delete with key value
        prev->next = cur->next;
		pthread_mutex_unlock(&list->lk);
		rcu_synchronize(&rcu_global, d);
        free(cur);
    }
    else{
		pthread_mutex_unlock(&list->lk);
    }

    return ret;
}

int list_find(int key, spinlock_list_t *list, struct rcu_data *d)
{
    node_t *prev, *cur;
    int ret = 0, val = -1;

	rcu_reader_lock(&rcu_global, d);
    for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
    {
        if ((val = cur->value) >= key)
            break;
        
    }
    ret = (val == key);
	rcu_reader_unlock(&rcu_global, d);

    return ret;
}

void *test(void* param)
{
    int op, bucket, value;
    value = 1;//sys_uptime();
	struct rcu_data self;

    thread_param_t *p_data = (thread_param_t*)param; 
    hash_list_t *p_hash_list = p_data->p_hash_list;
	self.id = p_data->tidx;
    rcu_register(&rcu_global, &self);

    pthread_barrier_wait(&bar);

    while (*(p_data->stop) == 0)
    {
        op = randomrange(1, 1000);
        value = randomrange(1, p_data->range);
        bucket = HASH_VALUE(p_hash_list, value);
        spinlock_list_t *p_list = p_hash_list->buckets[bucket];

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
			    if (list_delete(value, p_list, &self))
                {
                    p_data->variation--;
                }
                p_data->result_remove++;
            }
        }
        else
        {
            if(list_find(value, p_list, &self))
            {
                p_data->result_contains++;
            }
            p_data->result_found++;
        }
    }

    printf("thread %d end\n", getpid());
    
    return NULL;
}

int main(int argc, char **argv)
{
    thread_param_t *param_list;
    hash_list_t *p_hash_list;
    int *thread_list;
    int stop = 0;
    unsigned long exp = 0, total_variation = 0, total_size = 0;
    unsigned long reads = 0, updates = 0;
    unsigned long iv = 0, fv = 0;

	int n_buckets = DEFAULT_BUCKETS;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	int duration = DEFAULT_DURATION;
	int update = DEFAULT_UPDATE;
	int range = DEFAULT_RANGE;

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
    printf("-Nb threads   : %d\n", nb_threads);
    printf("-Initial size : %d\n", initial);
    printf("-Buckets      : %d\n", n_buckets);
    printf("-Duration     : %d\n", duration);
    printf("-Update rate  : %d\n", update);
    printf("-range        : %d\n", range);
    printf("-Set type     : hash-list\n");

	assert(n_buckets >= 1);
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

    for (int i = 0; i < p_hash_list->n_buckets; i++) {
        spinlock_list_t *list;
        list = (spinlock_list_t *)malloc(sizeof(spinlock_list_t));
        if (list == NULL) {
            printf("spinlock_list init error\n");
            exit(1);
        }
        list->head = NULL;
        list->bucket_id = i;
		pthread_mutex_init(&list->lk, NULL);
        p_hash_list->buckets[i] = list;
    }

    printf("initialize %d nodes...", initial);
    int j = 0;
    while (j < initial)
    {
        int value = randomrange(1, range);
        int bucket = HASH_VALUE(p_hash_list, value);

        if (list_insert(value, p_hash_list->buckets[bucket]))
        {
            j++;
        }
    }
    printf("done\n");

    thread_list = (int *)malloc(nb_threads*sizeof(int));
    if (thread_list == NULL) {
        printf("thread_list init error\n");
        exit(1);
    }
    rcu_init(&rcu_global, nb_threads);

    param_list = (thread_param_t *)malloc(nb_threads*sizeof(thread_param_t));
    if (param_list == NULL) {
        printf("param_list init error\n");
        exit(1);
    }
   
    pthread_barrier_init(&bar, 0, nb_threads);
   
    printf("Creating %d threads...", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        param_list[i].n_buckets = n_buckets;
        param_list[i].initial = initial;
        param_list[i].nb_threads = nb_threads;
        param_list[i].tidx = i;
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
    for(int i = 0; i < nb_threads; i++)
    {
        pthread_join(thread_list[i], 0);
    }
    printf(" done!\n");

    printf("\n####result####\n");
	for (int i = 0; i < nb_threads; i++) {
		printf("Thread %d\n", i);
		printf("  #add        : %ld\n", param_list[i].result_add);
		printf("  #remove     : %ld\n", param_list[i].result_remove);
		printf("  #contains   : %ld\n", param_list[i].result_contains);
		printf("  #found      : %ld\n", param_list[i].result_found);
		reads += param_list[i].result_contains;
		updates += (param_list[i].result_add + param_list[i].result_remove);
		total_variation += param_list[i].variation;
	}

    total_size = 0;
    for(int i = 0; i < n_buckets; i++)
    {
        node_t *node = p_hash_list->buckets[i]->head;
        while(node != NULL)
        {
            node = node->next;
            total_size++;
        }
    }
    exp = initial + total_variation;

    printf("Set size      : %ld (expected: %ld)\n", total_size, exp);
    printf("Duration      : %d (ms)\n", duration);
    iv = (reads + updates) * 1000.0 / duration;
    fv = (int)((reads + updates) * 1000.0 / duration * 10) % 10;
    printf("#ops          : %ld (%ld.%ld / s)\n", reads + updates, iv, fv);
    iv = reads * 1000.0 / duration;
    fv = (int)(reads * 1000.0 / duration * 10) % 10;
    printf("#read ops     : %ld (%ld.%ld / s)\n", reads, iv, fv);
    iv = updates * 1000.0 / duration;
    fv = (int)(updates * 1000.0 / duration * 10) % 10;
    printf("#update ops   : %ld (%ld.%ld / s)\n", updates, iv, fv);

    if(exp != total_size)
    {
		printf("\n<<<<<< ASSERT FAILURE(%ld!=%ld) <<<<<<<<\n", exp, total_size);
    }
    printf("benchlist_rcu end\n");
    exit(0);
}
