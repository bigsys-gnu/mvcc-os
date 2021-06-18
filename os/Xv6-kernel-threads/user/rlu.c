/////////////////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////////////////

# define likely(x) __builtin_expect ((x), 1)
# define unlikely(x) __builtin_expect ((x), 0)

#include "rlu.h"
#include "user.h"

/////////////////////////////////////////////////////////////////////////////////////////
// DEFINES - GENERAL
/////////////////////////////////////////////////////////////////////////////////////////

#define MAX_VERSION (LONG_MAX-1)

#define LOCK_ID(th_id) (th_id + 1)

#define WS_INDEX(ws_counter) ((ws_counter) % RLU_MAX_WRITE_SETS)

#define ALIGN_NUMBER (8)
#define ALIGN_MASK (ALIGN_NUMBER-1)
#define PERFORM_ALIGNMENT(obj_size) (obj_size + (ALIGN_NUMBER - (obj_size & ALIGN_MASK)))
#define ALIGN_OBJ_SIZE(obj_size) ((obj_size & ALIGN_MASK) ? PERFORM_ALIGNMENT(obj_size) : obj_size)

#define MOVE_PTR_FORWARD(p_obj, offset) ((void *)(((volatile unsigned char *)p_obj) + offset))
#define MOVE_PTR_BACK(p_obj, offset) ((void *)(((volatile unsigned char *)p_obj) - offset))

#define OBJ_HEADER_SIZE (sizeof(rlu_obj_header_t))
#define WS_OBJ_HEADER_SIZE (sizeof(rlu_ws_obj_header_t))

#define OBJ_TO_H(p_obj) ((volatile rlu_obj_header_t *)MOVE_PTR_BACK(p_obj, OBJ_HEADER_SIZE))
#define H_TO_OBJ(p_h_obj) ((volatile void *)MOVE_PTR_FORWARD(p_h_obj, OBJ_HEADER_SIZE))
#define OBJ_COPY_TO_WS_H(p_obj_copy) ((volatile rlu_ws_obj_header_t *)MOVE_PTR_BACK(p_obj_copy, (OBJ_HEADER_SIZE+WS_OBJ_HEADER_SIZE)))

#define PTR_ID_OBJ_COPY ((void *)0x12341234)

#define GET_COPY(p_obj) (OBJ_TO_H(p_obj)->p_obj_copy)

#define PTR_IS_LOCKED(p_obj_copy) (p_obj_copy != NULL)
#define PTR_IS_COPY(p_obj_copy) (p_obj_copy == PTR_ID_OBJ_COPY)

#define PTR_GET_WS_HEADER(p_obj_copy) (OBJ_COPY_TO_WS_H(p_obj_copy))

#define WS_GET_THREAD_ID(p_ws_obj_header) (p_ws_obj_header->thread_id)
#define WS_GET_RUN_COUNTER(p_ws_obj_header) (p_ws_obj_header->run_counter)

#define IS_UNLOCKED(p_obj) (!PTR_IS_LOCKED(GET_COPY(p_obj)))
#define IS_COPY(p_obj) (PTR_IS_COPY(GET_COPY(p_obj)))
#define GET_THREAD_ID(p_obj) (WS_GET_THREAD_ID(PTR_GET_WS_HEADER(GET_COPY(p_obj))))

#define GET_ACTUAL(p_obj_copy) (PTR_GET_WS_HEADER(p_obj_copy)->p_obj_actual)
#define FORCE_ACTUAL(p_obj) (IS_COPY(p_obj) ? GET_ACTUAL(p_obj) : p_obj)

#define TRY_CAS_PTR_OBJ_COPY(p_obj, new_ptr_obj_copy) (CAS((volatile int *)&(OBJ_TO_H(p_obj)->p_obj_copy), 0, (int)new_ptr_obj_copy) == 0)
#define UNLOCK(p_obj) OBJ_TO_H(p_obj)->p_obj_copy = NULL

#define RLU_CACHE_LINE_SIZE (32)

#ifdef __x86_64
# define RLU_CACHE_LINE_SIZE (64)
#elif defined(__PPC64__)
# define RLU_CACHE_LINE_SIZE (128)
#endif

/////////////////////////////////////////////////////////////////////////////////////////
// DEFINES - ATOMICS
/////////////////////////////////////////////////////////////////////////////////////////
/* TODO for PPC */
#define CPU_RELAX() asm volatile("pause\n": : :"memory");
#define MEMBARSTLD() __sync_synchronize()
#define FETCH_AND_ADD(addr, v) __sync_fetch_and_add((addr), (v))
#define CAS(addr, expected_value, new_value) __sync_val_compare_and_swap((addr), (expected_value), (new_value))

#ifndef smp_swap
#define smp_swap(__ptr, __val)			\
	__sync_lock_test_and_set(__ptr, __val)
#endif

/////////////////////////////////////////////////////////////////////////////////////////
// DEFINES - ASSERT AND DEBUG
/////////////////////////////////////////////////////////////////////////////////////////

#define RLU_ASSERT(cond) \
    if (unlikely(!(cond))) { \
	  printf (1, "\n-----------------------------------------------\n"); \
	  printf (1, "\nAssertion failure: %s:%d '%s'\n", __FILE__, __LINE__, #cond); \
	  abort();															\
    }

#define RLU_ASSERT_MSG(cond, self, fmt, ...) \
    if (unlikely(!(cond))) { \
	  printf (1, "\n-----------------------------------------------\n"); \
	  printf (1, "\nAssertion failure: %s:%d '%s'\n", __FILE__, __LINE__, #cond); \
	  abort();															\
    }

#define Q_ITERS_LIMIT (100000000)

/////////////////////////////////////////////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
// GLOBALS
/////////////////////////////////////////////////////////////////////////////////////////

static volatile int g_rlu_type = 0;
static volatile int g_rlu_max_write_sets = 0;

static volatile long g_rlu_cur_threads = 0;
static volatile rlu_thread_data_t *g_rlu_threads[RLU_MAX_THREADS] = {0,};

static volatile long g_rlu_writer_locks[RLU_MAX_WRITER_LOCKS] = {0,};

#ifndef RLU_ORDO_TIMESTAMPING
static volatile unsigned long g_rlu_array[RLU_CACHE_LINE_SIZE * 64] = {0,};

#define g_rlu_writer_version g_rlu_array[RLU_CACHE_LINE_SIZE * 2]
#define g_rlu_commit_version g_rlu_array[RLU_CACHE_LINE_SIZE * 4]
#endif


/////////////////////////////////////////////////////////
// HELPER FUNCTIONS
/////////////////////////////////////////////////////////
static void abort(void)
{
  exit();
}

/////////////////////////////////////////////////////////////////////////////////////////
// INTERNAL FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////
// Writer locks
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_reset_writer_locks(rlu_thread_data_t *self, long ws_id) {
	self->obj_write_set[ws_id].writer_locks.size = 0;
}

static void rlu_add_writer_lock(rlu_thread_data_t *self, long writer_lock_id) {
	int i;
	long n_locks;
	
	n_locks = self->obj_write_set[self->ws_cur_id].writer_locks.size;
	for (i = 0; i < n_locks; i++) {
		RLU_ASSERT(self->obj_write_set[self->ws_cur_id].writer_locks.ids[i] != writer_lock_id);
	}
	
	self->obj_write_set[self->ws_cur_id].writer_locks.ids[n_locks] = writer_lock_id;
	self->obj_write_set[self->ws_cur_id].writer_locks.size++;
	
	RLU_ASSERT(self->obj_write_set[self->ws_cur_id].writer_locks.size < RLU_MAX_NESTED_WRITER_LOCKS);
}

static int rlu_try_acquire_writer_lock(rlu_thread_data_t *self, long writer_lock_id) {
	volatile long cur_lock;
	
	cur_lock = g_rlu_writer_locks[writer_lock_id];
	if (cur_lock == 0) {
		if (CAS(&g_rlu_writer_locks[writer_lock_id], 0, LOCK_ID(self->uniq_id)) == 0) {
			return 1;
		}
	}
	
	return 0;	
}

static void rlu_release_writer_lock(rlu_thread_data_t *self, long writer_lock_id) {
	RLU_ASSERT(g_rlu_writer_locks[writer_lock_id] == LOCK_ID(self->uniq_id));
	
	g_rlu_writer_locks[writer_lock_id] = 0;
}

static void rlu_release_writer_locks(rlu_thread_data_t *self, int ws_id) {
	int i;
	
	for (i = 0; i < self->obj_write_set[ws_id].writer_locks.size; i++) {
		rlu_release_writer_lock(self, self->obj_write_set[ws_id].writer_locks.ids[i]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Write-set processing
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_reset(rlu_thread_data_t *self) {
	self->is_write_detected = 0;
	self->is_steal = 1;
	self->is_check_locks = 1;

}

static void rlu_reset_write_set(rlu_thread_data_t *self, long ws_counter) {
	long ws_id = WS_INDEX(ws_counter);

	self->obj_write_set[ws_id].num_of_objs = 0;
	self->obj_write_set[ws_id].p_cur = (void *)&(self->obj_write_set[ws_id].buffer[0]);
	
	rlu_reset_writer_locks(self, ws_id);
}

static void *rlu_add_ws_obj_header_to_write_set(rlu_thread_data_t *self, void *p_obj, obj_size_t obj_size) {
	void *p_cur;
	rlu_ws_obj_header_t *p_ws_obj_h;
	rlu_obj_header_t *p_obj_h;

	p_cur = (void *)self->obj_write_set[self->ws_cur_id].p_cur;

	p_ws_obj_h = (rlu_ws_obj_header_t *)p_cur;

	p_ws_obj_h->p_obj_actual = p_obj;
	p_ws_obj_h->obj_size = obj_size;
	p_ws_obj_h->run_counter = self->run_counter;
	p_ws_obj_h->thread_id = self->uniq_id;

	p_cur = MOVE_PTR_FORWARD(p_cur, WS_OBJ_HEADER_SIZE);

	p_obj_h = (rlu_obj_header_t *)p_cur;
	p_obj_h->p_obj_copy = PTR_ID_OBJ_COPY;

	p_cur = MOVE_PTR_FORWARD(p_cur, OBJ_HEADER_SIZE);

	self->obj_write_set[self->ws_cur_id].p_cur = p_cur;

	return p_cur;

}

static void rlu_add_obj_copy_to_write_set(rlu_thread_data_t *self, void *p_obj, obj_size_t obj_size) {
	void *p_cur;
	long cur_ws_size;

	p_cur = (void *)self->obj_write_set[self->ws_cur_id].p_cur;

	memcpy((unsigned char *)p_cur, (unsigned char *)p_obj, obj_size);

	p_cur = MOVE_PTR_FORWARD(p_cur, ALIGN_OBJ_SIZE(obj_size));

	self->obj_write_set[self->ws_cur_id].p_cur = p_cur;
	self->obj_write_set[self->ws_cur_id].num_of_objs++;

	cur_ws_size = (long)p_cur - (long)self->obj_write_set[self->ws_cur_id].buffer;
	RLU_ASSERT(cur_ws_size < RLU_MAX_WRITE_SET_BUFFER_SIZE);
}

static void rlu_writeback_write_set(rlu_thread_data_t *self, long ws_counter) {
	unsigned int i;
	long ws_id;
	obj_size_t obj_size;
	void *p_cur;
	void *p_obj_copy;
	void *p_obj_actual;
	rlu_ws_obj_header_t *p_ws_obj_h;
	rlu_obj_header_t *p_obj_h;

	ws_id = WS_INDEX(ws_counter);

	p_cur = (void *)&(self->obj_write_set[ws_id].buffer[0]);

	for (i = 0; i < self->obj_write_set[ws_id].num_of_objs; i++) {

		p_ws_obj_h = (rlu_ws_obj_header_t *)p_cur;

		p_obj_actual = (void *)p_ws_obj_h->p_obj_actual;
		obj_size = (obj_size_t)p_ws_obj_h->obj_size;

		p_cur = MOVE_PTR_FORWARD(p_cur, WS_OBJ_HEADER_SIZE);
		p_obj_h = (rlu_obj_header_t *)p_cur;

		RLU_ASSERT(p_obj_h->p_obj_copy == PTR_ID_OBJ_COPY);

		p_cur = MOVE_PTR_FORWARD(p_cur, OBJ_HEADER_SIZE);

		p_obj_copy = (void *)p_cur;

		memcpy((unsigned char *)p_obj_actual, (unsigned char *)p_obj_copy, obj_size);

		p_cur = MOVE_PTR_FORWARD(p_cur, ALIGN_OBJ_SIZE(obj_size));

		RLU_ASSERT_MSG(GET_THREAD_ID(p_obj_actual) == self->uniq_id,
			self, "th_id = %d my_id = %d\n p_obj_actual = %p num_of_objs = %u\n",
			GET_THREAD_ID(p_obj_actual), self->uniq_id, p_obj_actual, self->obj_write_set[ws_id].num_of_objs);

		UNLOCK(p_obj_actual);

	}

	RLU_ASSERT(p_cur == self->obj_write_set[ws_id].p_cur);
}

static int rlu_writeback_write_sets_and_unlock(rlu_thread_data_t *self) {
	long ws_wb_num;
	long ws_counter;

	for (ws_counter = self->ws_head_counter; ws_counter < self->ws_wb_counter; ws_counter++) {
		rlu_reset_write_set(self, ws_counter);
	}

	self->ws_head_counter = self->ws_wb_counter;

#ifdef RLU_TIME_MEASUREMENT
	self->t_begin = __read_tsc();
#endif
	ws_wb_num = 0;
	for (ws_counter = self->ws_wb_counter; ws_counter < self->ws_tail_counter; ws_counter++) {
		rlu_writeback_write_set(self, ws_counter);
		ws_wb_num++;
	}
#ifdef RLU_TIME_MEASUREMENT
	self->t_end = __read_tsc();
	self->t_writeback_spent += self->t_end - self->t_begin; 
#endif
	self->ws_wb_counter = self->ws_tail_counter;

	return ws_wb_num;

}

static void rlu_unlock_objs(rlu_thread_data_t *self, int ws_counter) {
	unsigned int i;
	long ws_id;
	obj_size_t obj_size;
	void *p_cur;
	void *p_obj_actual;
	rlu_ws_obj_header_t *p_ws_obj_h;
	rlu_obj_header_t *p_obj_h;

	ws_id = WS_INDEX(ws_counter);

	p_cur = (void *)&(self->obj_write_set[ws_id].buffer[0]);

	for (i = 0; i < self->obj_write_set[ws_id].num_of_objs; i++) {

		p_ws_obj_h = (rlu_ws_obj_header_t *)p_cur;

		p_obj_actual = (void *)p_ws_obj_h->p_obj_actual;
		obj_size = p_ws_obj_h->obj_size;

		p_cur = MOVE_PTR_FORWARD(p_cur, WS_OBJ_HEADER_SIZE);
		p_obj_h = (rlu_obj_header_t *)p_cur;

		RLU_ASSERT(p_obj_h->p_obj_copy == PTR_ID_OBJ_COPY);

		p_cur = MOVE_PTR_FORWARD(p_cur, OBJ_HEADER_SIZE);

		RLU_ASSERT(GET_COPY(p_obj_actual) == p_cur);

		p_cur = MOVE_PTR_FORWARD(p_cur, ALIGN_OBJ_SIZE(obj_size));

		UNLOCK(p_obj_actual);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Thread asserts
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_assert_in_section(rlu_thread_data_t *self) {
	RLU_ASSERT(self->run_counter & 0x1);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Thread register and unregister
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_register_thread(rlu_thread_data_t *self) {
	RLU_ASSERT((self->run_counter & 0x1) == 0);

	FETCH_AND_ADD(&self->run_counter, 1);
	self->local_version = g_rlu_writer_version;
	self->local_commit_version = g_rlu_commit_version;
}

static void rlu_unregister_thread(rlu_thread_data_t *self) {
	RLU_ASSERT((self->run_counter & 0x1) != 0);

	self->run_counter++;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Free buffer processing
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_process_free(rlu_thread_data_t *self) {
	int i;
	void *p_obj;


	for (i = 0; i < self->free_nodes_size; i++) {
		p_obj = self->free_nodes[i];

		RLU_ASSERT_MSG(IS_UNLOCKED(p_obj),
			self, "object is locked. p_obj = %p th_id = %d\n",
			p_obj, GET_THREAD_ID(p_obj));

		free((void *)OBJ_TO_H(p_obj));
	}

	self->free_nodes_size = 0;

}

/////////////////////////////////////////////////////////////////////////////////////////
// Sync
/////////////////////////////////////////////////////////////////////////////////////////
static void rlu_init_quiescence(rlu_thread_data_t *self) {
	int th_id;

	MEMBARSTLD();

	for (th_id = 0; th_id < g_rlu_cur_threads; th_id++) {

		self->q_threads[th_id].is_wait = 0;

		if (th_id == self->uniq_id) {
			// No need to wait for myself
			continue;
		}

		if (g_rlu_threads[th_id] == NULL) {
			// No need to wait for uninitialized threads
			continue;
		}

		self->q_threads[th_id].run_counter = g_rlu_threads[th_id]->run_counter;

		if (self->q_threads[th_id].run_counter & 0x1) {
			// The other thread is running -> wait for the thread
			self->q_threads[th_id].is_wait = 1;	
		}
	}
}

static long rlu_wait_for_quiescence(rlu_thread_data_t *self, unsigned long version_limit)
{
  int th_id;
	long iters;
	long cur_threads;

	iters = 0;
	cur_threads = g_rlu_cur_threads;
	for (th_id = 0; th_id < cur_threads; th_id++) {

		while (self->q_threads[th_id].is_wait) {
			iters++;

			if (self->q_threads[th_id].run_counter != g_rlu_threads[th_id]->run_counter) {
				self->q_threads[th_id].is_wait = 0;
				break;
			}

			if (version_limit) {
				if (g_rlu_threads[th_id]->local_version >= version_limit) {
					self->q_threads[th_id].is_wait = 0;
					break;
				}
			}

			if (iters > Q_ITERS_LIMIT) {
				iters = 0;
				printf(1, "[%d] waiting for [%d] with: local_version = %d , run_cnt = %d\n", self->uniq_id, th_id,
					   g_rlu_threads[th_id]->local_version, g_rlu_threads[th_id]->run_counter);
			}

			CPU_RELAX();

		}
	}


	return iters;
}

static void rlu_synchronize(rlu_thread_data_t *self) {

	if (self->is_no_quiescence) {
		return;
	}

	rlu_init_quiescence(self);
	rlu_wait_for_quiescence(self, self->writer_version);
}

static void rlu_sync_and_writeback(rlu_thread_data_t *self) {
	long ws_num;
	long ws_wb_num;
	
	RLU_ASSERT((self->run_counter & 0x1) == 0);

	if (self->ws_tail_counter == self->ws_head_counter) {
		return;
	}


	ws_num = self->ws_tail_counter - self->ws_wb_counter;

	self->writer_version = g_rlu_writer_version + 1;
	FETCH_AND_ADD(&g_rlu_writer_version, 1);
	/* # NOTE 1.
	 * - Update interval of an object should be greater than
	 * 2x ordo boundary to dereference consistent snapshot
	 * without abort.
	 *
	 * # NOTE 2.
	 * - It is better to update self->writer_version
	 * using an atomic swap operation to immediately
	 * make its change public. */

	rlu_synchronize(self);

	ws_wb_num = rlu_writeback_write_sets_and_unlock(self);

	RLU_ASSERT_MSG(ws_num == ws_wb_num, self, "failed: %d != %d\n", ws_num, ws_wb_num);

	self->writer_version = MAX_VERSION;

	FETCH_AND_ADD(&g_rlu_commit_version, 1);

	if (self->is_sync) {
		self->is_sync = 0;
	}

	rlu_process_free(self);

}

static void rlu_send_sync_request(int other_th_id) {
	g_rlu_threads[other_th_id]->is_sync++;
	MEMBARSTLD();
}

static void rlu_commit_write_set(rlu_thread_data_t *self) {
	// Move to the next write-set
	self->ws_tail_counter++;
	self->ws_cur_id = WS_INDEX(self->ws_tail_counter);

	// Sync and writeback when:
	// (1) All write-sets are full
	// (2) Aggregared MAX_ACTUAL_WRITE_SETS
	if ((WS_INDEX(self->ws_tail_counter) == WS_INDEX(self->ws_head_counter)) ||
		((self->ws_tail_counter - self->ws_wb_counter) >= self->max_write_sets)) {
		rlu_sync_and_writeback(self);
	}

	RLU_ASSERT(self->ws_tail_counter > self->ws_head_counter);
	RLU_ASSERT(WS_INDEX(self->ws_tail_counter) != WS_INDEX(self->ws_head_counter));
}

/////////////////////////////////////////////////////////////////////////////////////////
// EXTERNAL FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////
void rlu_init_args(int type, int ws) {
	g_rlu_writer_version = 0;
	g_rlu_commit_version = 0;

	if (type == RLU_TYPE_COARSE_GRAINED) {
		g_rlu_type = RLU_TYPE_COARSE_GRAINED;
		g_rlu_max_write_sets = 1;
		printf(1, "RLU - COARSE_GRAINED initialized\n");
	} else if (type == RLU_TYPE_FINE_GRAINED) {
		g_rlu_type = RLU_TYPE_FINE_GRAINED;
		g_rlu_max_write_sets = ws;
		printf(1, "RLU - FINE_GRAINED initialized [max_write_sets = %d]\n", g_rlu_max_write_sets);
	} else {
		exit();
	}

	RLU_ASSERT(RLU_MAX_WRITE_SETS >= 2);
	RLU_ASSERT(ws >= 1);
	RLU_ASSERT(ws * 2 <= RLU_MAX_WRITE_SETS);

}

void rlu_init(void) {
	g_rlu_writer_version = 0;
	g_rlu_commit_version = 0;

	if (RLU_TYPE == RLU_TYPE_COARSE_GRAINED) {
		g_rlu_type = RLU_TYPE_COARSE_GRAINED;
		g_rlu_max_write_sets = 1;
		printf(1, "RLU - COARSE_GRAINED initialized\n");
	} else if (RLU_TYPE == RLU_TYPE_FINE_GRAINED) {
		g_rlu_type = RLU_TYPE_FINE_GRAINED;
		g_rlu_max_write_sets = RLU_NUM_WS;
		printf(1, "RLU - FINE_GRAINED initialized [max_write_sets = %d]\n", g_rlu_max_write_sets);
	} else {
		abort();
	}

	RLU_ASSERT(RLU_MAX_WRITE_SETS >= 2);
	RLU_ASSERT(RLU_NUM_WS >= 1);
	RLU_ASSERT(RLU_NUM_WS * 2 <= RLU_MAX_WRITE_SETS);
}


void rlu_finish(void) { }

void rlu_thread_init(rlu_thread_data_t *self) {
	int ws_counter;

	memset(self, 0, sizeof(rlu_thread_data_t));

	self->type = g_rlu_type;
	self->max_write_sets = g_rlu_max_write_sets;

	self->uniq_id = FETCH_AND_ADD(&g_rlu_cur_threads, 1);

	self->local_version = 0;
	self->writer_version = MAX_VERSION;

	for (ws_counter = 0; ws_counter < RLU_MAX_WRITE_SETS; ws_counter++) {
		rlu_reset_write_set(self, ws_counter);
	}

	g_rlu_threads[self->uniq_id] = self;
	MEMBARSTLD();

}

void *rlu_alloc(obj_size_t obj_size) {
	void *ptr;
	rlu_obj_header_t *p_obj_h;

	ptr = (void *)malloc(OBJ_HEADER_SIZE + obj_size);
	if (ptr == NULL) {
		return NULL;
	}
	p_obj_h = (rlu_obj_header_t *)ptr;
	p_obj_h->p_obj_copy = NULL;

	return (void *)H_TO_OBJ(p_obj_h);
}

void rlu_free(rlu_thread_data_t *self, void *p_obj) {
	if (p_obj == NULL) {
		return;
	}

	if (self == NULL) {
		free((void *)OBJ_TO_H(p_obj));
		return;
	}
	
	rlu_assert_in_section(self);
	
	p_obj = (void *)FORCE_ACTUAL(p_obj);

	self->free_nodes[self->free_nodes_size] = p_obj;
	self->free_nodes_size++;

	RLU_ASSERT(self->free_nodes_size < RLU_MAX_FREE_NODES);

}

void rlu_sync_checkpoint(rlu_thread_data_t *self) {

	if (likely(!self->is_sync)) {
		return;
	}

	rlu_sync_and_writeback(self);

}

void rlu_reader_lock(rlu_thread_data_t *self) {

	rlu_sync_checkpoint(self);

	rlu_reset(self);

	rlu_register_thread(self);

	self->is_steal = 1;
	if ((self->local_version - self->local_commit_version) == 0) {
		self->is_steal = 0;
	}

	self->is_check_locks = 1;

	if (((self->local_version - self->local_commit_version) == 0) && ((self->ws_tail_counter - self->ws_wb_counter) == 0)) {
		self->is_check_locks = 0;
	}
}

int rlu_try_writer_lock(rlu_thread_data_t *self, int writer_lock_id) {
	RLU_ASSERT(self->type == RLU_TYPE_COARSE_GRAINED);
	
	if (!rlu_try_acquire_writer_lock(self, writer_lock_id)) {
		return 0;
	}
	
	rlu_add_writer_lock(self, writer_lock_id);
	
	return 1;
}

void rlu_reader_unlock(rlu_thread_data_t *self) {

	rlu_unregister_thread(self);

	if (self->is_write_detected) {
		self->is_write_detected = 0;
		rlu_commit_write_set(self);
		rlu_release_writer_locks(self, WS_INDEX(self->ws_tail_counter - 1));
	} else {
		rlu_release_writer_locks(self, self->ws_cur_id);
		rlu_reset_writer_locks(self, self->ws_cur_id);
	} 
	
	rlu_sync_checkpoint(self);

}

void *rlu_deref_slow_path(rlu_thread_data_t *self, void *p_obj) {
	long th_id;
	void *p_obj_copy;
	volatile rlu_ws_obj_header_t *p_ws_obj_h;

	if (p_obj == NULL) {
		return p_obj;
	}

	p_obj_copy = (void *)GET_COPY(p_obj);

	if (!PTR_IS_LOCKED(p_obj_copy)) {
		return p_obj;
	}

	if (PTR_IS_COPY(p_obj_copy)) {
		// p_obj points to a copy -> it has been already dereferenced.
		return p_obj;
	}

	p_ws_obj_h = PTR_GET_WS_HEADER(p_obj_copy);

	th_id = WS_GET_THREAD_ID(p_ws_obj_h);

	if (th_id == self->uniq_id) {
		// p_obj is locked by this thread -> return the copy
		return p_obj_copy;
	}

	// p_obj is locked by another thread
	if (self->is_steal) {
		if (g_rlu_threads[th_id]->writer_version <= self->local_version) {
			// This thread started after the other thread updated g_writer_version.
			// and this thread observed a valid p_obj_copy (!= NULL)
			// => The other thread is going to wait for this thread to finish before reusing the write-set log
			//    (to which p_obj_copy points)
			return p_obj_copy;
		}
	}

	return p_obj;
}

int rlu_try_lock(rlu_thread_data_t *self, void **p_p_obj, size_t obj_size) {
	void *p_obj;
	void *p_obj_copy;
	volatile long th_id;
	volatile rlu_ws_obj_header_t *p_ws_obj_h;

	p_obj = *p_p_obj;

	RLU_ASSERT_MSG(p_obj != NULL, self, "[%d] rlu_try_lock: tried to lock a NULL pointer\n",
		       self->writer_version);

	p_obj_copy = (void *)GET_COPY(p_obj);

	if (PTR_IS_COPY(p_obj_copy)) {

		p_obj = (void *)GET_ACTUAL(p_obj);
		p_obj_copy = (void *)GET_COPY(p_obj);

	}

	if (PTR_IS_LOCKED(p_obj_copy)) {
		p_ws_obj_h = PTR_GET_WS_HEADER(p_obj_copy);

		th_id = WS_GET_THREAD_ID(p_ws_obj_h);

		if (th_id == self->uniq_id) {
			if (self->run_counter == WS_GET_RUN_COUNTER(p_ws_obj_h)) {
				// p_obj already locked by current execution of this thread.
				// => return copy
				*p_p_obj = p_obj_copy;
				return 1;
			}

			// p_obj is locked by another execution of this thread.
			self->is_sync++;
			return 0;
		}

		// p_obj already locked by another thread.
		// => send sync request to the other thread
		// => in the meantime -> sync this thread

		rlu_send_sync_request(th_id);
		self->is_sync++;
		return 0;
	}

	// p_obj is free

	// Indicate that write-set is updated
	if (self->is_write_detected == 0) {
		self->is_write_detected = 1;
		self->is_check_locks = 1;
	}

	// Add write-set header for the object
	p_obj_copy = rlu_add_ws_obj_header_to_write_set(self, p_obj, obj_size);

	// Try lock p_obj -> install pointer to copy
	if (!TRY_CAS_PTR_OBJ_COPY(p_obj, p_obj_copy)) {
		return 0;
	}

	// Locked successfully

	// Copy object to write-set
	rlu_add_obj_copy_to_write_set(self, p_obj, obj_size);

	RLU_ASSERT_MSG(GET_COPY(p_obj) == p_obj_copy,
		self, "p_obj_copy = %p my_p_obj_copy = %p\n",
		GET_COPY(p_obj), p_obj_copy);

	*p_p_obj = p_obj_copy;
	return 1;
}

void rlu_lock(rlu_thread_data_t *self, void **p_p_obj, unsigned int obj_size) {
	RLU_ASSERT(self->type == RLU_TYPE_COARSE_GRAINED);
	
	RLU_ASSERT(rlu_try_lock(self, p_p_obj, obj_size) != 0);
}

void rlu_abort(rlu_thread_data_t *self) {

	rlu_unregister_thread(self);

	if (self->is_write_detected) {
		self->is_write_detected = 0;
		rlu_unlock_objs(self, self->ws_tail_counter);
		
		rlu_release_writer_locks(self, self->ws_cur_id);
		rlu_reset_write_set(self, self->ws_tail_counter);
	} else {
		rlu_release_writer_locks(self, self->ws_cur_id);
		rlu_reset_writer_locks(self, self->ws_cur_id);
	}

	rlu_sync_checkpoint(self);
}

int rlu_cmp_ptrs(void *p_obj_1, void *p_obj_2) {
	if (p_obj_1 != NULL) {
		p_obj_1 = (void *)FORCE_ACTUAL(p_obj_1);
	}

	if (p_obj_2 != NULL) {
		p_obj_2 = (void *)FORCE_ACTUAL(p_obj_2);
	}

	return p_obj_1 == p_obj_2;
}

void rlu_assign_pointer(void **p_ptr, void *p_obj) {
	if (p_obj != NULL) {
		p_obj = (void *)FORCE_ACTUAL(p_obj);
	}

	*p_ptr = p_obj;
}
