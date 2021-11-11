#ifndef _CONFIG_H
#define _CONFIG_H
#include "arch.h"

#define DISABLE_MUNMAP
#define MVRLU_ORDO_TIMESTAMPING	// ENABLE ORDO TIMESTAMP

#define MVRLU_LOG_SIZE (1ul << 17) /* 128KB */
#define MVRLU_LOG_MASK (~(MVRLU_LOG_SIZE - 1))
#define MVRLU_MAX_THREAD_NUM (1ul << 7) /* 128 (total 16GB LOG) */

#define MVRLU_MAX_FREE_PTRS 512
#define MVRLU_QP_INTERVAL_USEC 500 /* 0.5 msec */

#define MVRLU_LOG_LOW_MARK (MVRLU_LOG_SIZE >> 1) /* 50% */
#define MVRLU_LOG_HIGH_MARK (MVRLU_LOG_SIZE - (MVRLU_LOG_SIZE >> 2)) /* 75% */
#define MVRLU_DEREF_MIN_ACT_OBJ 50
#define MVRLU_DEREF_MARK 3

#define MVRLU_CACHE_LINE_SIZE L1_CACHE_BYTES
#define MVRLU_CACHE_LINE_MASK (~(MVRLU_CACHE_LINE_SIZE - 1))
#define MVRLU_DEFAULT_PADDING CACHE_DEFAULT_PADDING
//#define MVRLU_NESTED_LOCKING
//#define MVRLU_ENABLE_STATS
#endif /* _CONFIG_H */
