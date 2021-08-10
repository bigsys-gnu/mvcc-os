#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "mmu.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "vm.hh"
#include "kmtrace.hh"
#include "futex.h"
#include "version.hh"
#include "filetable.hh"
#include "cpputil.hh"

#include <uk/mman.h>
#include <uk/utsname.h>
#include <uk/unistd.h>

#define MAX_BUCKETS (128)
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

enum SYN_TYPE {
    SPINLOCK,
    RCU,
    RLU,
    MVRLU,
};
/////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////
uint64_t
u_rand(void)
{
  uint64_t t = rdtsc() / 2;
  return (t >= 0) ? t : 0 - t;
}

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
  seed[0] = (unsigned short)u_rand();
  seed[1] = (unsigned short)u_rand();
  seed[2] = (unsigned short)u_rand();
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

    NEW_DELETE_OPS(node);
} node_t;

typedef struct list {
    node_t *head;

    list(void): head(NULL) {}

    virtual
    ~list(void) {
        for (node_t *iter  = head; iter != NULL;)
        {
            node_t *trash = iter;
            iter = iter->next;
            delete trash;
        }
    }

    // insert without lock
    int raw_insert(int key) {
        node_t *prev = NULL, *cur = NULL, *new_node = NULL;
        int ret = 1;

        if(head == NULL)
        {
            new_node = new node_t;
            new_node->next = NULL;
            new_node->value = key;
            head = new_node;

            return ret;
        }

        prev = head;
        cur = prev->next;
        for (; cur != NULL; prev = cur, cur = cur->next)
        {
            if (cur->value == key)
            {
                ret = 0;
                break;
            }
        }

        if(ret){
            //no node with key value
            new_node = new node_t;
            new_node->next = NULL;
            new_node->value = key;
            prev->next = new_node;
        }

        return ret;
    }
    virtual int list_insert(int key) = 0;
    virtual int list_delete(int key) = 0;
    virtual int list_find(int key) = 0;

} list_t;


struct spinlock_list : public list {
    spinlock lk;

    spinlock_list(void): list(), lk("list_lock") {}

    int list_insert(int key) override {
        scoped_acquire l(&lk);
        return raw_insert(key);
    }

    int list_delete(int key) override {
        node_t *prev, *cur;
        int ret = 0;

        scoped_acquire l(&lk);

        if(head == NULL){
            return ret;
        }
        else{
            prev = head;
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
            delete cur;
            // cprintf( "delete node value : %d\tpid : %d\n", key, getpid());
        }
        // else{
        // cprintf( "nothing to delete %d\t pid : %d\n", key, getpid());
        // }
        return ret;
    }

    int list_find(int key) override {
        node_t *prev = NULL, *cur = NULL;
        int ret = 0, val;

        scoped_acquire l(&lk);

        for (prev = head, cur = prev->next; cur != NULL;
             prev = cur, cur = cur->next)
        {
            if ((val = cur->value) == key)
            {
                ret = (val == key);
                break;
            }
        }
        return ret;
    }

    NEW_DELETE_OPS(spinlock_list);
};

list_t *
get_list(enum SYN_TYPE type)
{
    switch(type){
    case SPINLOCK:
        return static_cast<list *>(new spinlock_list);
    case RCU:
    case RLU:
    case MVRLU:
        return NULL;
    default:
        return NULL;
    }
}

typedef struct hash_list {
    int n_buckets;
    list_t *buckets[MAX_BUCKETS];  

    hash_list(int n_buc, int initial, int range, enum SYN_TYPE type): n_buckets(n_buc) {
        for (int i = 0; i < n_buckets; i++) {
            list_t *list = get_list(type);
            if (list == NULL) {
                cprintf("spinlock_list init error\n");
                return;
            }
            buckets[i] = list;
        }
        cprintf("initialize %d nodes...", initial);
        int j = 0;
        unsigned short seed[3];

        rand_init(seed);
        while (j < initial)
        {
            int value = rand_range(range, seed);
            int bucket = HASH_VALUE(this, value);

            if (buckets[bucket]->raw_insert(value))
            {
                j++;
            }
        }
        cprintf("done\n");
    }

    ~hash_list(void) {
        for (int i = 0; i < n_buckets; i++)
        {
            delete buckets[i];
        }
    }
    NEW_DELETE_OPS(hash_list);
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
    unsigned short seed[3];

    thread_param(int n_buckets, int initial, int nb_threads, int update, int range,
                 int *stop, hash_list_t *p_hash_list)
        :n_buckets(n_buckets), initial(initial), nb_threads(nb_threads), update(update),
         range(range), variation(0), result_add(0), result_remove(0),
         result_contains(0), result_found(0), stop(stop), p_hash_list(p_hash_list)
        {
            rand_init(seed);
        }

    NEW_DELETE_OPS(thread_param);
} thread_param_t;


void test(void* param)
{
    int op, bucket, value;
    value = 1;
    cprintf("thread %d Start\n", myproc()->pid);


    thread_param_t *p_data = (thread_param_t*)param; 
    hash_list_t *p_hash_list = p_data->p_hash_list;

    while (*p_data->stop == 0)
    {
        op = rand_range(1000, p_data->seed);
        value = rand_range(p_data->range, p_data->seed);
        bucket = HASH_VALUE(p_hash_list, value);
        list_t *p_list = p_hash_list->buckets[bucket];
        // p_list = p_list;
        if (op < p_data->update)
        {
            if ((op & 0x01) == 0)
            {
                if (p_list->list_insert(value))
                {
                    p_data->variation++;
                }
                p_data->result_add++;
            }
            else
            {
                if (p_list->list_delete(value))
                {
                    p_data->variation--;
                }
                p_data->result_remove++;
            }
        }
        else
        {
            if(p_list->list_find(value))
            {
                p_data->result_contains++;
            }
            p_data->result_found++;
        }
    }
}



//SYSCALL
void
sys_benchmark(int th, int init, int buck, int dur, int upd, int rng)
{
    cprintf("Run Kernel Level Benchmark\n");


    thread_param_t **param_list;
    hash_list_t *p_hash_list;
    struct proc** thread_list;
    int stop = 0;
    u64 initial_time = 0;
    unsigned long exp = 0, total_variation = 0, total_size = 0;
    unsigned long reads = 0, updates = 0;
    unsigned long iv = 0, fv = 0;

	int n_buckets = buck;
	int initial = init;
	int nb_threads = th;
	int duration = dur;
	int update = upd;
	int range = rng;

    assert(n_buckets >= 1);
    assert(duration >= 0);
    assert(initial >= 0);
	assert(nb_threads > 0);
	assert(update >= 0 && update <= 1000);
	assert(range > 0 && range >= initial);

    p_hash_list = new hash_list(n_buckets, initial, range, SPINLOCK);

    thread_list = new struct proc *[nb_threads];
    if (thread_list == NULL) {
        cprintf("thread_list init error\n");
        return;
    }

    param_list = new thread_param *[nb_threads];
    if (param_list == NULL) {
        cprintf("param_list init error\n");
        return;
    }
   
    initial_time = nsectime();
    cprintf("Main thread ID: %d\n", myproc()->pid);
    cprintf("Creating %d threads...", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        param_list[i] = new thread_param(n_buckets, initial, nb_threads, update, range,
                                         &stop, p_hash_list);
    }

    for (int i = 0; i < nb_threads; i++)
    {
        thread_list[i] = threadpin(test, (void*)param_list[i], "test_thread", i%(ncpu-1)+1);
        cprintf("\nThread created %p(c:%d, s:%d)\n", thread_list[i], i%(ncpu-1)+1, thread_list[i]->get_state());
    }

    cprintf(" done!\n");

    {
        struct spinlock tmp("sleep lock");
        struct condvar cond("sleep cond");
        u64 until = initial_time + duration * 1000000;

        scoped_acquire l(&tmp);
        while(until > nsectime())
            cond.sleep_to(&tmp, until);

        stop = 1;
    }

    cprintf("join %d threads...\n", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        wait(-1, NULL);
    }
    cprintf(" done!\n");

    cprintf( "\n####result####\n");
	for (int i = 0; i < nb_threads; i++) {
		cprintf( "Thread %d\n", i);
		cprintf( "  #add        : %d\n", param_list[i]->result_add);
		cprintf( "  #remove     : %d\n", param_list[i]->result_remove);
		cprintf( "  #contains   : %d\n", param_list[i]->result_contains);
		cprintf( "  #found      : %d\n", param_list[i]->result_found);
		reads += param_list[i]->result_found;
		updates += (param_list[i]->result_add + param_list[i]->result_remove);
		total_variation += param_list[i]->variation;
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
    cprintf( "\n#### B ####\n");

    cprintf( "Set size      : %d (expected: %d)\n", (int)total_size, (int)exp);
    cprintf( "Duration      : %d (ms)\n", (int)duration);
    iv = (reads + updates) * 1000.0 / duration;
    fv = (int)((reads + updates) * 1000.0 / duration * 10) % 10;
    cprintf( "#ops          : %d (%d.%d / s)\n", (int)(reads + updates), (int)iv, (int)fv);
    iv = reads * 1000.0 / duration;
    fv = (int)(reads * 1000.0 / duration * 10) % 10;
    cprintf( "#read ops     : %d (%d.%d / s)\n", (int)reads, (int)iv, (int)fv);
    iv = updates * 1000.0 / duration;
    fv = (int)(updates * 1000.0 / duration * 10) % 10;
    cprintf( "#update ops   : %d (%d.%d / s)\n", (int)updates, (int)iv, (int)fv);

    if(exp != total_size)
    {
		cprintf("\n<<<<<< ASSERT FAILURE(%d!=%d) <<<<<<<<\n", (int)exp, (int)total_size);
    }

    // free kernel memory usage
    delete p_hash_list;
    delete[] thread_list;
    for (int i = 0; i < nb_threads; i++)
    {
        delete param_list[i];
    }
    delete[] param_list;

    cprintf("Kernel Level Benchmark END\n");
}
