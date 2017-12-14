/*
 * latch.h
 *
 *  Created on: 2017年10月19日
 *      Author: linzer
 */

#ifndef __QNODE_LATCH_H__
#define __QNODE_LATCH_H__

#include <condition.h>
#include <atomic.h>

typedef struct {
	condition cond;
	atomic_t count;
	mutex lock;
} latch;

static inline void latch_init(latch *lh, int count) {
	atomic_set(&lh->count, count);
	MUTEX_INIT(lh);
	condition_init(&lh->cond, &lh->lock);
}

static inline int latch_count_get(latch *lh) {
	return atomic_get(&lh->count);
}

/* 返回操作之后计数器的值 */
static inline int latch_count_down(latch *lh) {
	MUTEX_LOCK(lh);
	int c = atomic_dec(&lh->count);
	if(0 == c) {
		condition_notify_all(&lh->cond);
	}
	MUTEX_UNLOCK(lh);

	return c;
}

static inline void latch_wait(latch *lh) {
	MUTEX_LOCK(lh);
	while (latch_count_get(lh) > 0) {
		condition_wait(&lh->cond);
	}
	MUTEX_UNLOCK(lh);
}

static inline void latch_destroy(latch *lh) {
	atomic_reset(&lh->count);
	MUTEX_DESTROY(lh);
	condition_destroy(&lh->cond);
}
#endif /* __QNODE_LATCH_H__ */
