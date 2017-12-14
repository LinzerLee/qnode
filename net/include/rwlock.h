/*
 * rwlock.h
 *
 *  Created on: 2017年10月17日
 *      Author: linzer
 */

#ifndef __QNODE_RWLOCK_H__
#define __QNODE_RWLOCK_H__

#include <define.h>

#define RWLOCK_INIT(q) 	rwlock_init(&(q)->lock)
#define READ_LOCK(q) 	rwlock_rlock(&(q)->lock)
#define READ_UNLOCK(q) 	rwlock_runlock(&(q)->lock)
#define WRITE_LOCK(q) 	rwlock_wlock(&(q)->lock)
#define WRITE_UNLOCK(q) rwlock_wunlock(&(q)->lock)
#define RWLOCK_DESTROY(q) rwlock_destroy(&(q)->lock)

#ifndef USE_PTHREAD_RWLOCK
#include <atomic.h>

typedef struct {
	int write;
	int read;
} rwlock;

static inline void
rwlock_init(rwlock *lock) {
	lock->write = 0;
	lock->read = 0;
}

static inline void
rwlock_rlock(rwlock *lock) {
	for (;;) {
		while(lock->write) {
			FULL_BARRIER();
		}
		ATOM_INC_NEW(&lock->read);
		if (lock->write) {
			ATOM_DEC_NEW(&lock->read);
		} else {
			break;
		}
	}
}

static inline void
rwlock_wlock(rwlock *lock) {
	while (ATOM_SET_OLD(&lock->write,1)) {}
	while(lock->read) {
		FULL_BARRIER();
	}
}

static inline void
rwlock_wunlock(rwlock *lock) {
	ATOM_RESET(&lock->write);
}

static inline void rwlock_runlock(rwlock *lock) {
	ATOM_DEC_NEW(&lock->read);
}

static inline void rwlock_destroy(rwlock *lock) {
	IGNORE(lock);
}

#else

#include <pthread.h>

typedef struct {
	pthread_rwlock_t lock;
} rwlock;

static inline void
rwlock_init(rwlock *lock) {
	pthread_rwlock_init(&lock->lock, NULL);
}

static inline void
rwlock_rlock(rwlock *lock) {
	 pthread_rwlock_rdlock(&lock->lock);
}

static inline void rwlock_wlock(rwlock *lock) {
	 pthread_rwlock_wrlock(&lock->lock);
}

static inline void rwlock_wunlock(rwlock *lock) {
	pthread_rwlock_unlock(&lock->lock);
}

static inline void rwlock_runlock(rwlock *lock) {
	pthread_rwlock_unlock(&lock->lock);
}

static inline void rwlock_destroy(rwlock *lock) {
	IGNORE(lock);
}
#endif /* USE_PTHREAD_RWLOCK */

#endif /* __QNODE_RWLOCK_H__ */
