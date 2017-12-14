/*
 * spinlock.h
 *
 *  Created on: 2017年10月17日
 *      Author: linzer
 */

#ifndef __QNODE_SPINLOCK_H__
#define __QNODE_SPINLOCK_H__

#include <define.h>

#define SPIN_INIT(q) spinlock_init(&(q)->lock)
#define SPIN_LOCK(q) spinlock_lock(&(q)->lock)
#define SPIN_UNLOCK(q) spinlock_unlock(&(q)->lock)
#define SPIN_DESTROY(q) spinlock_destroy(&(q)->lock)

#ifndef USE_PTHREAD_SPINLOCK
#include <atomic.h>

typedef struct {
  int lock;
} spinlock;

static inline void spinlock_init(spinlock *lock) {
	lock->lock = 0;
}

static inline void spinlock_lock(spinlock *lock) {
	while (ATOM_SET_OLD(&lock->lock,1)) {}
}

static inline int spinlock_trylock(spinlock *lock) {
	return ATOM_SET_OLD(&lock->lock,1) == 0;
}

static inline void spinlock_unlock(spinlock *lock) {
	ATOM_RESET(&lock->lock);
}

static inline void spinlock_destroy(spinlock *lock) {
	IGNORE(lock);
}

#else

#include <mutex.h>

typedef mutex spinlock;

static inline void spinlock_init(spinlock *lock) {
	mutex_init(lock);
}

static inline void spinlock_lock(spinlock *lock) {
	mutex_lock(lock);
}

static inline int spinlock_trylock(spinlock *lock) {
	return mutex_trylock(lock) == 0;
}

static inline void spinlock_unlock(spinlock *lock) {
	mutex_unlock(lock);
}

static inline void spinlock_destroy(spinlock *lock) {
	mutex_destroy(lock);
}

#endif /* USE_PTHREAD_SPINLOCK */

#endif /* __QNODE_SPINLOCK_H__ */
