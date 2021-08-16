typedef struct item {
  struct item *next;
  int value;
} item_t;

typedef struct list {
  item_t *first;
  mutex_t update_mtx;
} list_t;

int get_last_value(list_t *list)
{
  rcu_read_lock();				/* read critical section */
  item_t *cur = NULL;
  item_t *next = rcu_access(list->first); /* deref rcu protected */
  while (next)
	{
	  cur = next;
	  next = rcu_access(cur->next); /* deref rcu protected */
	}
  int val = (cur != NULL) ? cur->value : 0;
  rcu_read_unlock();			/* read critical section end */
  return val;
}

void insert_first(list_t *list, int val)
{
  item_t *item = (item_t*) malloc(sizeof(item_t));
  mutex_lock(&list->update_mtx); /* W-W lock */
  item->value = val;
  item->next = list->first;
  rcu_assign(list->first, item); /* apply data directly to memory */
  mutex_unlock(&list->update_mtx);
}

void delete_first(list_t *list)
{
  mutex_lock(&list->update_mtx); /* W-W lock */
  item_t *to_del = list->first;
  if (to_del)
	{
	  list->first = to_del->next;
	  mutex_unlock(&list->update_mtx);
	  rcu_synchronize();		/* wait for old readers */
	  free(to_del);
	}
  else
	mutex_unlock(&list->update_mtx);
}
