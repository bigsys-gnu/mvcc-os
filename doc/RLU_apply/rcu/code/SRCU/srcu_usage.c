idx = srcu_read_lock(&ss);
/* read-side ctitical section. */
srcu_read_unlock(&ss, idx);

list_del_rcu(p);
synchronize_srcu(&ss);
kfree(p);

int readside(void)
{
  int idx;
  rcu_read_lock();
  if (nomoresrcu)
	{
	  rcu_read_unlock();
	  return -EINVAL;
	}
  idx = srcu_read_lock(&ss);
  rcu_read_unlock();
  /* read-side critical. */
  srcu_read_unlock(&ss, idx);
  return 0;
}

void cleanup(void)
{
  nomoresrcu = 1;
  synchronize_rcu();
  synchronize_srcu(&ss);
  cleanup_srcu_struct(&ss);
}
