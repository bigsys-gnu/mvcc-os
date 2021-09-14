while (p = kmalloc(sizeof(*p), GFP_ATOMIC))
	call_rcu(&p->rcu, f);

while (p = kmalloc(sizeof(*p), GFP_ATOMIC))
	{
		synchronize_rcu();
		kfree(&p->rcu, f);
	}
