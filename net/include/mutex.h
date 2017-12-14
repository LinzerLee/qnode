/*
 * mutex.h
 *
 *  Created on: 2017年10月19日
 *      Author: linzer
 */

#ifndef __QNODE_MUTEX_H__
#define __QNODE_MUTEX_H__

#include <sys/types.h>

#include <pthread.h>
#include <assert.h>
#define MUTEX_INIT(q) mutex_init(&(q)->lock)
#define MUTEX_LOCK(q) mutex_lock(&(q)->lock)
#define MUTEX_UNLOCK(q) mutex_unlock(&(q)->lock)
#define MUTEX_DESTROY(q) mutex_destroy(&(q)->lock)

typedef struct{
	pthread_mutex_t lock;
	pid_t holder;
} mutex;

static inline void mutex_init(mutex *lock) {
	pthread_mutex_init(&lock->lock, NULL);
	lock->holder = 0;
}

extern int thread_current_id();

static inline void mutex_lock(mutex *lock) {
	pthread_mutex_lock(&lock->lock);
	lock->holder = thread_current_id();
}

static inline int mutex_trylock(mutex *lock) {
	if(pthread_mutex_trylock(&lock->lock) == 0) {
		lock->holder = thread_current_id();

		return 1;
	}

	return 0;
}

static inline void mutex_unlock(mutex *lock) {
	lock->holder = 0;
	pthread_mutex_unlock(&lock->lock);
}

static inline void mutex_destroy(mutex *lock) {
	assert(lock->holder == 0);
	pthread_mutex_destroy(&lock->lock);
}

static inline int mutex_locked_by_self(mutex *lock) {
	return lock->holder == thread_current_id();
}

static inline void mutex_assert_locked(mutex *lock) {
	return assert(mutex_locked_by_self(lock));
}
#endif /* __QNODE_MUTEX_H__ */
