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

#include <uk/mman.h>
#include <uk/utsname.h>
#include <uk/unistd.h>

#define MAX_BUCKETS (128)
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define HASH_VALUE(p_hash_list, val)       (val % p_hash_list->n_buckets)

typedef struct node {
    int value;
    struct node *next;
} node_t;

typedef struct list {
    spinlock lk;
    node_t *head;
} list_t;

typedef struct hash_list {
	int n_buckets;
	list_t *buckets[MAX_BUCKETS];  
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

int
u_rand(void)
{
  int t = rdtsc();
  return (t >= 0) ? t : 0 - t;
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
  return u_rand() % (range) + lo;
}

int list_insert(int key, list_t *list)
{
    node_t *prev = NULL, *cur = NULL, *new_node = NULL;
    int ret = 1;

    list->lk.acquire();

    if(list->head == NULL)
    {
        new_node = (node_t*)kmalloc(sizeof(node_t), "node");
        new_node->next = NULL;
        new_node->value = key;
        list->head = new_node;

        list->lk.release();
        return ret;
    }

    prev = list->head;
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
        new_node = (node_t*)kmalloc(sizeof(node_t), "node");
        new_node->next = NULL;
        new_node->value = key;
        prev->next = new_node;
    }

    list->lk.release();

    return ret;
}

int list_delete(int key, list_t *list)
{
    node_t *prev, *cur;
    int ret = 0;
    list->lk.acquire();

    if(list->head == NULL){
        list->lk.release();
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
        kmfree((void*)cur, sizeof(node_t));
        // cprintf( "delete node value : %d\tpid : %d\n", key, getpid());
    }
    else{
        // cprintf( "nothing to delete %d\t pid : %d\n", key, getpid());
    }
    list->lk.release();

    return ret;
}

int list_find(int key, list_t *list)
{
    node_t *prev = NULL, *cur = NULL;
    int ret = 0, val;

    list->lk.acquire();

    for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
    {
        if ((val = cur->value) == key)
        {
            ret = (val == key);
        }
    }
    list->lk.release();

    return ret;
}

void test(void* param)
{
    int op, bucket, value;
    value = 1;
    cprintf("thread %d Start\n", myproc()->pid);


    thread_param_t *p_data = (thread_param_t*)param; 
    hash_list_t *p_hash_list = p_data->p_hash_list;

    while (*p_data->stop == 0)
    {
        op = randomrange(1, 1000);
        value = randomrange(1, p_data->range);
        bucket = HASH_VALUE(p_hash_list, value);
        list_t *p_list = p_hash_list->buckets[bucket];
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
    }
}



//SYSCALL
void
sys_benchmark(int th, int init, int buck, int dur, int upd, int rng)
{
    cprintf("Run Kernel Level Benchmark\n");


    thread_param_t *param_list;
    hash_list_t *p_hash_list;
    struct proc** thread_list;
    int stop = 0;
    int initial_time = 0;
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

    p_hash_list = (hash_list_t *)kmalloc(sizeof(hash_list_t), "hashlist");
    if (p_hash_list == NULL) {
	    cprintf("hash_list init error\n");
	    return;
	}
    p_hash_list->n_buckets = n_buckets;

    for (int i = 0; i < p_hash_list->n_buckets; i++) {
        list_t *list;
        list = (list_t *)kmalloc(sizeof(list_t), "lists");
        if (list == NULL) {
            cprintf("spinlock_list init error\n");
            return;
        }
        list->head = NULL;
        // lock_init(&list->lk);
        list->lk = spinlock("list_lock");
        p_hash_list->buckets[i] = list;
    }

    cprintf("initialize %d nodes...", initial);
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
    cprintf("done\n");

    thread_list = (struct proc**)kmalloc(nb_threads*sizeof(struct proc*), "threads");
    if (thread_list == NULL) {
        cprintf("thread_list init error\n");
        return;
    }

    param_list = (thread_param_t *)kmalloc(nb_threads*sizeof(thread_param_t), "params");
    if (param_list == NULL) {
        cprintf("param_list init error\n");
        return;
    }
   
    initial_time = (int)(nsectime() / 1000000);
    cprintf("Main thread ID: %d\n", myproc()->pid);
    cprintf("Creating %d threads...", nb_threads);
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

        thread_list[i] = threadpin(test, (void*)&param_list[i], "test_thread", i%(ncpu-1)+1);
        cprintf("\nThread created %p(c:%d, s:%d)\n", thread_list[i], i%(ncpu-1)+1, thread_list[i]->get_state());
    }
    cprintf(" done!\n");

    while(1)
    {
        if((nsectime() / 1000000) - initial_time > duration)
        {
            stop = 1;
            cprintf( "elapsed time: %dms\n", (int)((nsectime() / 1000000) - initial_time));
            break;
        }
        // cprintf( "elapsed time: %dms\n", (int)(nsectime() / 1000000));
    }

    cprintf("join %d threads...\n", nb_threads);
    for(int i = 0; i < nb_threads; i++)
    {
        wait(-1, NULL);
        // thread_join();
        // sleep(0);
    }
    cprintf(" done!\n");

    cprintf( "\n####result####\n");
	for (int i = 0; i < nb_threads; i++) {
		cprintf( "Thread %d\n", i);
		cprintf( "  #add        : %d\n", param_list[i].result_add);
		cprintf( "  #remove     : %d\n", param_list[i].result_remove);
		cprintf( "  #contains   : %d\n", param_list[i].result_contains);
		cprintf( "  #found      : %d\n", param_list[i].result_found);
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



    cprintf("Kernel Level Benchmark END\n");
}
