/*
 * atomic.h
 *
 *  Created on: 2017年10月17日
 *      Author: linzer
 */

#ifndef __QNODE_ATOMIC_H__
#define __QNODE_ATOMIC_H__

#define ATOM_CAS_BOOL(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)
#define ATOM_CAS_OLD(ptr, oval, nval) __sync_val_compare_and_swap(ptr, oval, nval)
#define ATOM_SET_OLD(ptr, nval) __sync_lock_test_and_set(ptr, nval)
#define ATOM_RESET(ptr) __sync_lock_release(ptr)
#define ATOM_INC_NEW(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOM_INC_OLD(ptr) __sync_fetch_and_add(ptr, 1)
#define ATOM_DEC_NEW(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOM_DEC_OLD(ptr) __sync_fetch_and_sub(ptr, 1)
#define ATOM_ADD_NEW(ptr,n) __sync_add_and_fetch(ptr, n)
#define ATOM_ADD_OLD(ptr,n) __sync_fetch_and_add(ptr, n)
#define ATOM_SUB_NEW(ptr,n) __sync_sub_and_fetch(ptr, n)
#define ATOM_SUB_OLD(ptr,n) __sync_fetch_and_sub(ptr, n)
#define ATOM_AND_NEW(ptr,n) __sync_and_and_fetch(ptr, n)
#define ATOM_AND_OLD(ptr,n) __sync_fetch_and_and(ptr, n)
#define ATOM_OR_NEW(ptr,n) __sync_or_and_fetch(ptr, n)
#define ATOM_OR_OLD(ptr,n) __sync_fetch_and_or(ptr, n)
#define ATOM_XOR_NEW(ptr,n) __sync_xor_and_fetch(ptr, n)
#define ATOM_XOR_OLD(ptr,n) __sync_fetch_and_xor(ptr, n)
#define ATOM_NAND_NEW(ptr,n) __sync_nand_and_fetch(ptr, n)
#define ATOM_NAND_OLD(ptr,n) __sync_fetch_and_nand(ptr, n)

#define FULL_BARRIER() __sync_synchronize()

typedef struct {
    volatile int counter;
} atomic_t;

static inline int atomic_inc(atomic_t *ptr) {
	return ATOM_INC_NEW(&ptr->counter);
}

static inline int atomic_dec(atomic_t *ptr) {
	return ATOM_DEC_NEW(&ptr->counter);
}

static inline int atomic_add(atomic_t *ptr, int n) {
	return ATOM_ADD_NEW(&ptr->counter, n);
}

static inline int atomic_sub(atomic_t *ptr, int n) {
	return ATOM_SUB_NEW(&ptr->counter, n);
}

static inline int atomic_cas(atomic_t *ptr, int o, int n) {
	return ATOM_CAS_BOOL(&ptr->counter, o, n);
}

static inline int atomic_get(const atomic_t *ptr) {
	return ptr->counter;
}

static inline int atomic_set(atomic_t *ptr, int v) {
	return ATOM_SET_OLD(&ptr->counter, v);
}

static inline void atomic_reset(atomic_t *ptr) {
	ATOM_RESET(&ptr->counter);
}

typedef struct {
    volatile void *ptr;
} atomic_ptr;

static inline int atomic_ptr_cas(atomic_ptr *ptr, void *o, void *n) {
	return ATOM_CAS_BOOL(&ptr->ptr, o, n);
}

static inline void *atomic_ptr_get(const atomic_ptr *ptr) {
	return (void *)ptr->ptr;
}

static inline void *atomic_ptr_set(atomic_ptr *ptr, void *p) {
	return (void *)ATOM_SET_OLD(&ptr->ptr, p);
}

static inline void atomic_ptr_reset(atomic_ptr *ptr) {
	ATOM_RESET(&ptr->ptr);
}

#endif /* __QNODE_ATOMIC_H__ */
