#include "types.h"
#include "user.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "x86.h"

#undef NULL
#define NULL ((void *)0)

#define PGSIZE (4096)

#define INSERT_TIME 8000
#define DELETE_TIME 2000

typedef struct node
{
    int value;
    struct node *next;
} node_t;

typedef struct spinlock_list
{
    long id;
    lock_t *lk;
    node_t *head;
} spinlock_list_t;

int list_deletes()
{
    return 0;
}

uint random(void)
{
    // Take from http://stackoverflow.com/questions/1167253/implementation-of-rand
    static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
    unsigned int b;
    b = ((z1 << 6) ^ z1) >> 13;
    z1 = ((z1 & 4294967294U) << 18) ^ b;
    b = ((z2 << 2) ^ z2) >> 27;
    z2 = ((z2 & 4294967288U) << 2) ^ b;
    b = ((z3 << 13) ^ z3) >> 21;
    z3 = ((z3 & 4294967280U) << 7) ^ b;
    b = ((z4 << 3) ^ z4) >> 12;
    z4 = ((z4 & 4294967168U) << 13) ^ b;

    return (z1 ^ z2 ^ z3 ^ z4) / 2;
}

// Return a random integer between a given range.
int randomrange(int lo, int hi)
{
    if (hi < lo)
    {
        int tmp = lo;
        lo = hi;
        hi = tmp;
    }
    int range = hi - lo + 1;
    return random() % (range) + lo;
}

void list_insert(int key, spinlock_list_t *list)
{
    node_t *prev, *cur, *new_node;
    int ret = 1;

    urcu_writer_lock(list->id);
    if (list->head == NULL)
    {
        new_node = (node_t *)malloc(sizeof(node_t));
        new_node->next = NULL;
        new_node->value = key;
        list->head = new_node;
    }
    else
    {
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
    }

    if (ret)
    {
        //no node with key value
        new_node = (node_t *)malloc(sizeof(node_t));
        new_node->next = NULL;
        new_node->value = key;
        prev->next = new_node;
        printf(1, "insert_node value : %d\tpid : %d\n", key, getpid());
    }
    else
    {
        printf(1, "node exist value : %d\tpid : %d\n", key, getpid());
    }
    urcu_writer_unlock(list->id);

    return;
}

void list_delete(int key, spinlock_list_t *list)
{
    node_t *prev, *cur;
    int ret = 0;
    // lock_acquire(list->lk);
    urcu_writer_lock(list->id);

    if (list->head == NULL)
    {
        return;
    }
    else
    {
        prev = list->head;
        cur = prev->next;
        for (; cur != NULL; prev = cur, cur = cur->next)
        {
            if (cur->value == key)
            {
                printf(1, "delete value : %d\n", cur->value);
                ret = 1;
                break;
            }
        }
    }

    if (ret)
    {
        //node to delete with key value
        prev->next = cur->next;
        free(cur);
        printf(1, "delete node value : %d\tpid : %d\n", key, getpid());
    }
    else
    {
        printf(1, "nothing to delete %d\t pid : %d\n", key, getpid());
    }
    // lock_release(list->lk);
    urcu_writer_unlock(list->id);

    return;
}

int list_find(int key, spinlock_list_t *list)
{
    node_t *prev, *cur;
    int ret, val;

    // lock_acquire(list->lk);
    urcu_reader_lock();
    for (prev = list->head, cur = prev->next; cur != NULL; prev = cur, cur = cur->next)
    {
        if ((val = cur->value) >= key)
            break;
        ret = (val == key);
        printf(1, "ret: %d\n", ret);
        // lock_release(list->lk);
        urcu_reader_unlock();
    }
    exit();
}

void test(void *ptr)
{
    int i, value;
    value = 1; //sys_uptime();
    int insert_time = 0;
    int delete_time = 0;

    // temporary.. need to implement gathering thread id
    printf(1, "1");
    // long unique_id = getpid();
    printf(1, "2");
    // urcu_register(unique_id);
    printf(1, "3");

    for (i = 0; i < 100; i++)
    {
        printf(1, "for i : %d\n", i);

        value = randomrange(1, 30);
        if (i == 0 || i % 2 == 0)
        {
            list_insert(value, (spinlock_list_t *)ptr);
            insert_time++;
        }
        else
        {
            list_delete(value, (spinlock_list_t *)ptr);
            delete_time++;
        }
        value++;
    }
    printf(1, "thread %d end\n", getpid());
    exit();
}

int main()
{
    spinlock_list_t *list = (spinlock_list_t *)malloc(sizeof(spinlock_list_t));
    list->head = NULL;
    list->id = 0;

    printf(1, "bench_list main\n");
    lock_init(list->lk);

    /* urcu initialize */
    urcu_init(2);

    int thread_pid1 = thread_create(&test, (void *)list);
    int thread_pid2 = thread_create(&test, (void *)list);
    printf(1, "pid1 = %d\n", thread_pid1);
    printf(1, "pid2 = %d\n", thread_pid2);
    assert(thread_pid1 > 0);
    assert(thread_pid2 > 0);

    int join_pid1 = thread_join();
    int join_pid2 = thread_join();
    assert(join_pid1 == thread_pid1);
    assert(join_pid2 == thread_pid2);

    printf(1, "bench_list end\n");

    exit();
}
