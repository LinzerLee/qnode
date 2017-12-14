/*
 * array.h
 *
 *  Created on: 2017年10月20日
 *      Author: linzer
 */

#ifndef __QNODE_ARRAY_H__
#define __QNODE_ARRAY_H__
#include <spinlock.h>
#include <rwlock.h>

typedef struct {
	spinlock lock;
	void *value;
} array_node;

typedef struct {
	rwlock lock;
	size_t capacity;
	size_t size;
	array_node *set;
	int enable_lock; /* 结点锁开启标志 */
} array;

#define ARRAY_DEFAULT_CAPACITY	(1 << 5)

#define ARRAY_INDEX_LOCK(arr, index)								\
	do {                                                        	\
		READ_LOCK((arr));										\
		if((arr)->enable_lock) {									\
			if((arr)->size > (index))							\
				SPIN_LOCK(&(arr)->set[(index)]);					\
		}														\
	}                                                           	\
	while (0)

#define ARRAY_INDEX_UNLOCK(arr, index)							\
	do {                                         				\
		READ_UNLOCK((arr));										\
		if((arr)->enable_lock) {									\
			if((arr)->size > (index))							\
				SPIN_UNLOCK(&(arr)->set[(index)]);				\
		}														\
	}                                                           	\
	while (0)

void array_enable_lock(array *arr);
void array_disable_lock(array *arr);
size_t array_calc_capacity(size_t cap);
array *array_create_capacity(size_t capacity);
array *array_create();
size_t array_capacity(const array *arr);
size_t array_size(const array *arr);
int array_add(array *arr, void *object);
void *array_remove(array *arr, size_t index);
/* 将数组容量修改为指定大小--即使不成功也有可能删除数据 */
int array_resize(array *arr, size_t size);
/* 严格紧凑数组 */
int array_strict_compact(array *arr);
/* 紧凑数组 */
int array_compact(array *arr);
/* 该函数需要被宏ARRAY_INDEX_LOCK和ARRAY_INDEX_UNLOCK包围 */
void *array_index(array *arr, size_t index);
void array_clear(array *arr);
/* 非线程安全 */
void array_destroy(array **arr);

/* byte array */
typedef struct {
	char *block;
	size_t size;
	size_t capacity;
} byte_array;

byte_array *byte_array_create_buffer(char *buf, size_t size, size_t capacity);
byte_array *byte_array_create_capacity(size_t capacity);
byte_array *byte_array_create();
byte_array *byte_array_dup(byte_array *arr);
size_t byte_array_capacity(const byte_array *arr);
size_t byte_array_size(const byte_array *arr);
int byte_array_add(byte_array *arr, const char *obj, size_t size);
char *byte_array_offset(byte_array *arr, int size);
int byte_array_assgin(byte_array *arr, const char *obj, size_t size);
/* 返回操作结果 */
int byte_array_remove(byte_array *arr, size_t begin, size_t end);
/* 将数组容量修改为指定大小--即使不成功也有可能删除数据 */
int byte_array_resize(byte_array *arr, size_t size);
/* 严格紧凑数组 */
int byte_array_strict_compact(byte_array *arr);
/* 紧凑数组 */
int byte_array_compact(byte_array *arr);
char *byte_array_get_bytes(byte_array *arr);
void byte_array_clear(byte_array *arr);
void byte_array_swap(byte_array *arr1, byte_array *arr2);
/* 非线程安全 */
void byte_array_destroy(byte_array **arr);

/* 范型数组 */
#define ARRAY								byte_array *
#define ARRAY_CREATE(arr)					byte_array *arr=byte_array_create()
#define ARRAY_NEW(arr)						(arr=byte_array_create())
#define ARRAY_PUSH_BACK(arr, type, value)						\
do {																\
	type var = value;											\
	byte_array_add(arr, (const char *)&var, sizeof(type));		\
} while(0)
#define ARRAY_POP_BACK_PTR(arr, type)		ARRAY_OFFSET(arr, type, -1)
#define ARRAY_POP_BACK_REF(arr, type)		(*ARRAY_POP_BACK_PTR(arr, type))
#define ARRAY_BEGIN(arr, type)				ARRAY_AT_PTR(arr, type, 0)
#define ARRAY_END(arr, type)					ARRAY_AT_PTR(arr, type, ARRAY_SIZE(arr, type))
#define ARRAY_BACK(arr, type)				ARRAY_AT_PTR(arr, type, ARRAY_SIZE(arr, type) - 1)
#define ARRAY_AT_PTR(arr, type, index)		((type *)(byte_array_get_bytes(arr) + sizeof(type) * (index)))
#define ARRAY_AT_REF(arr, type, index)		(*ARRAY_AT_PTR(arr, type, index))
#define ARRAY_INDEX(ptr, arr, type)			(((const char *)ptr - (const char *)byte_array_get_bytes(arr)) / sizeof(type))
#define ARRAY_REMOVE(arr, type, index)		byte_array_remove(arr, sizeof(type) * (index), sizeof(type) * ((index) + 1) - 1)
#define ARRAY_DESTROY(arr)					byte_array_destroy(&arr)
#define ARRAY_CLEAR(arr)						byte_array_clear(arr)
#define ARRAY_SIZE(arr, type)				(byte_array_size(arr) / sizeof(type))
#define ARRAY_RESIZE(arr, type, size)		byte_array_resize(arr, sizeof(type) * (size))
#define ARRAY_CAPACITY(arr, type)			(byte_array_capacity(arr) / sizeof(type))
#define ARRAY_OFFSET(arr, type, num)			((type *)byte_array_offset(arr, (int)sizeof(type) * (num)))
#define ARRAY_FOREACH(ptr, arr, type)							\
	for(type *ptr = (type *)byte_array_get_bytes(arr); 			\
			ptr<((type *)byte_array_get_bytes(arr)+ARRAY_SIZE(arr, type)); ++ ptr)
typedef int (* ARRAY_SORT_FUNC)(const void *, const void *);
#define ARRAY_QSORT(arr, int, fcmp) qsort((char *)byte_array_get_bytes(arr), ARRAY_SIZE(arr, int), sizeof(int), fcmp)
#endif /* __QNODE_ARRAY_H__ */
