int init_srcu_struct(struct srcu_struct *sp);
void cleanup_srcu_struct(struct srcu_struct *sp);
int srcu_read_lock(struct srcu_struct *sp);
void srcu_read_unlock(struct srcu_struct *sp, int idx);
void synchronize_srcu(struct srcu_struct *sp);
long srcu_batches_completed(struct srcu_struct *sp); /* for testing */

struct srcu_struct_array {
	int c[2];						/* prev and cur */
};

struct srcu_struct {
	int completed;
	struct srcu_array *per_cpu_ref;
	struct mutex mutex;
};

int init_srcu_struct(struct srcu_struct *sp)
{
	sp->completed = 0;
	mutex_init(&sp->mutex);
	sp->per_cpu_ref = alloc_percpu(struct srcu_struct_array);
	return (sp->per_cpu_ref ? 0 : -ENOMEM);
}

int srcu_readers_active_idx(struct srcu_struct *sp, int idx)
{
	int cpu;
	int sum;
	
	sum = 0;
	for_each_possible_cpu(cpu)
		sum += per_cpu_ptr(sp->per_cpu_ref, cpu)->c[idx];
	return sum;
}
	
int srcu_readers_active(struct srcu_struct *sp)
{
	return srcu_readers_active_idx(sp, 0) +
	       srcu_readers_active_idx(sp, 1);
}
	
void cleanup_srcu_struct(struct srcu_struct *sp)
{
	int sum;
	
	sum = srcu_readers_active(sp);
	WARN_ON(sum);
	if (sum != 0)
		return;
	free_percpu(sp->per_cpu_ref);
	sp->per_cpu_ref = NULL;
}


int srcu_read_lock(struct srcu_struct *sp)
{
	int idx;
	
	preempt_disable();
	idx = sp->completed & 0x1;
	barrier();
	per_cpu_ptr(sp->per_cpu_ref, smp_processor_id())->c[idx]++;
	srcu_barrier();
	preempt_enable();	/* barrier call is embedded in */
	return idx;
}

void srcu_read_unlock(struct srcu_struct *sp, int idx)
{
	preempt_disable();
	srcu_barrier();
	per_cpu_ptr(sp->per_cpu_ref, smp_processor_id())->c[idx]--;
	preempt_enable();   /* barrier call is embedded in */
}

void synchronize_srcu(struct srcu_struct *sp) {
	int idx;

	idx = sp->completed;		/* take snapshot */
	mutex_lock(&sp->mutex);
	if ((sp->completed - idx) >= 2) { /* someone did it */
		mutex_unlock(&sp->mutex);	  /* instead of me. */
		return;
	}
	synchronize_sched();		/* wait every preempt_disable() section. */
	idx = sp->completed & 0x1;
	sp->completed++;
	synchronize_sched();
	while (srcu_readers_active_idx(sp, idx))
		schedule_timeout_interruptible(1);
	synchronize_sched();
	mutex_unlock(&sp->mutex);
}
