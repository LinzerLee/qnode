/*
 * condition.h
 *
 *  Created on: 2017年10月19日
 *      Author: linzer
 */

#ifndef __QNODE_CONDITION_H__
#define __QNODE_CONDITION_H__

#include <errno.h>
#include <pthread.h>

#include <mutex.h>

typedef struct{
	mutex *plock;
	pthread_cond_t cond;
} condition;

static inline void condition_init(condition *pcond, mutex *plock) {
	assert(pcond != NULL);
	assert(plock != NULL);
	pcond->plock = plock;
	pthread_cond_init(&pcond->cond, NULL);
}

static inline void condition_destroy(condition *pcond) {
	pcond->plock = NULL;
	pthread_cond_destroy(&pcond->cond);
}

static inline void condition_wait(condition *pcond) {
	pthread_cond_wait(&pcond->cond, &pcond->plock->lock);
}

static inline int
condition_wait_timeout(condition *pcond, double seconds) {
	struct timespec abstime;
	const int64_t kNanoSecondsPerSecond = 1e9;
	int64_t nanoseconds = (int64_t)(seconds * kNanoSecondsPerSecond);

	clock_gettime(CLOCK_REALTIME, &abstime);
	abstime.tv_sec += (time_t)((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
	abstime.tv_nsec = (long)((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);

	return ETIMEDOUT == pthread_cond_timedwait(&pcond->cond, &pcond->plock->lock, &abstime);;
}

static inline void condition_notify(condition *pcond) {
	pthread_cond_signal(&pcond->cond);
}

static inline void condition_notify_all(condition *pcond) {
	pthread_cond_broadcast(&pcond->cond);
}
#endif /* __QNODE_CONDITION_H__ */
