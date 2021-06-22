#include "types.h"
#include "user.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "x86.h"

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

typedef struct node {
    int value;
    struct node *next;
} node_t;

typedef struct spinlock_list {
    lock_t lk;
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
    int variation;
    int result_add;
    int result_remove;
    int result_contains;
    int result_found;
    int *stop;
    hash_list_t *p_hash_list;
} thread_param_t;

int list_deletes()
{
   return 0; 
}

uint random(void)
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

    // lock_acquire(&list->lk);

    if(list->head == NULL)
    {
        new_node = (node_t*)th_malloc(sizeof(node_t));
        new_node->next = NULL;
        new_node->value = key;
        list->head = new_node;
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
        new_node = (node_t *)th_malloc(sizeof(node_t));
        new_node->next = NULL;
        new_node->value = key;
        prev->next = new_node;
        // printf(1, "insert_node value : %d\tpid : %d\n", key, getpid());
    }
    else{
        // printf(1, "node exist value : %d\tpid : %d\n", key, getpid());
    }
    // lock_release(&list->lk);

    return ret;
}

int list_delete(int key, spinlock_list_t *list)
{
    node_t *prev, *cur;
    int ret = 0;
    // lock_acquire(&list->lk);
    if(list->head == NULL){
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
            // sleep(0);
        }
    }

    if(ret)
    {
        //node to delete with key value
        prev->next = cur->next;
        th_free(cur);
        // printf(1, "delete node value : %d\tpid : %d\n", key, getpid());
    }
    else{
        // printf(1, "nothing to delete %d\t pid : %d\n", key, getpid());
    }
    // lock_release(&list->lk);

    return ret;
}

int list_find(int key, spinlock_list_t *list)
{
    node_t *prev, *cur;
    int ret, val;

    // lock_acquire(&list->lk);
    for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
    {
        if ((val = cur->value) >= key)
            break;
        ret = (val == key);
        // sleep(0);
    }
    // lock_release(&list->lk);
    // sleep(0);

    return ret;
}

void test(void* param)
{
    int op, bucket, value;
    value = 1;//sys_uptime();

    thread_param_t *p_data = (thread_param_t*)param; 
    hash_list_t *p_hash_list = p_data->p_hash_list;

    while (*p_data->stop == 0)
    {
        op = randomrange(1, 1000);
        value = randomrange(1, p_data->range);
        bucket = HASH_VALUE(p_hash_list, value);
        spinlock_list_t *p_list = p_hash_list->buckets[bucket];
        // p_list = p_list;
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
            if(list_find(value, p_list))
            {
                p_data->result_contains++;
            }
            p_data->result_found++;
        }
        /* sleep(1); */
    }

    printf(1, "thread %d end\n", getpid());
    exit();
}

int main(int argc, char **argv)
{
    thread_param_t *param_list;
    hash_list_t *p_hash_list;
    int *thread_list;
    int stop = 0, initial_time = 0;
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
        printf(1, "%d Option is inserted\n", argc - 1);
        break;
    }
    printf(1, "-Nb threads   : %d\n", nb_threads);
    printf(1, "-Initial size : %d\n", initial);
    printf(1, "-Buckets      : %d\n", n_buckets);
    printf(1, "-Duration     : %d\n", duration);
    printf(1, "-Update rate  : %d\n", update);
    printf(1, "-range        : %d\n", range);
    printf(1,"-Set type     : hash-list\n");

	assert(n_buckets >= 1 && n_buckets <= MAX_BUCKETS);
	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(update >= 0 && update <= 1000);
	assert(range > 0 && range >= initial);

    p_hash_list = (hash_list_t *)th_malloc(sizeof(hash_list_t));
    if (p_hash_list == NULL) {
	    printf(1,"hash_list init error\n");
	    exit();
	}
    p_hash_list->n_buckets = n_buckets;

    for (int i = 0; i < p_hash_list->n_buckets; i++) {
        spinlock_list_t *list;
        list = (spinlock_list_t *)th_malloc(sizeof(spinlock_list_t));
        if (list == NULL) {
            printf(1,"spinlock_list init error\n");
            exit();
        }
        list->head = NULL;
        lock_init(&list->lk);
        p_hash_list->buckets[i] = list;
    }

    printf(1,"initialize %d nodes...", initial);
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
    printf(1,"done\n");

    thread_list = (int *)th_malloc(nb_threads*sizeof(int));
    if (thread_list == NULL) {
        printf(1,"thread_list init error\n");
        exit();
    }

    param_list = (thread_param_t *)th_malloc(nb_threads*sizeof(thread_param_t));
    if (param_list == NULL) {
        printf(1,"param_list init error\n");
        exit();
    }
   
    initial_time = uptime();
    printf(1,"Creating %d threads...", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        param_list[i].n_buckets = n_buckets;
        param_list[i].initial = initial;
        param_list[i].nb_threads = nb_threads;
        param_list[i].update = update;
        param_list[i].range = range;
        param_list[i].stop = &stop;
        param_list[i].variation = 0;
        param_list[i].p_hash_list = p_hash_list;

        thread_list[i] = thread_create(&test, (void*)&param_list[i]);
    }
    printf(1," done!\n");

    while(1)
    {
        if(uptime() - initial_time > duration / 10)
        {
            stop = 1;
            printf(1, "elapsed time: %dms\n", (uptime() - initial_time) * 10);
            break;
        }
        /* sleep(1); */
    }

    printf(1,"join %d threads...\n", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        thread_join();
        // sleep(0);
    }
    printf(1," done!\n");

    printf(1, "\n####result####\n");
	for (int i = 0; i < nb_threads; i++) {
		printf(1, "Thread %d\n", i);
		printf(1, "  #add        : %d\n", param_list[i].result_add);
		printf(1, "  #remove     : %d\n", param_list[i].result_remove);
		printf(1, "  #contains   : %d\n", param_list[i].result_contains);
		printf(1, "  #found      : %d\n", param_list[i].result_found);
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

    printf(1, "Set size      : %d (expected: %d)\n", total_size, exp);
    printf(1, "Duration      : %d (ms)\n", duration);
    iv = (reads + updates) * 1000.0 / duration;
    fv = (int)((reads + updates) * 1000.0 / duration * 10) % 10;
    printf(1, "#ops          : %d (%d.%d / s)\n", reads + updates, iv, fv);
    iv = reads * 1000.0 / duration;
    fv = (int)(reads * 1000.0 / duration * 10) % 10;
    printf(1, "#read ops     : %d (%d.%d / s)\n", reads, iv, fv);
    iv = updates * 1000.0 / duration;
    fv = (int)(updates * 1000.0 / duration * 10) % 10;
    printf(1, "#update ops   : %d (%d.%d / s)\n", updates, iv, fv);

    if(exp != total_size)
    {
		printf(1,"\n<<<<<< ASSERT FAILURE(%d!=%d) <<<<<<<<\n", exp, total_size);
    }
    printf(1, "benchlist end\n");
    exit();
}
