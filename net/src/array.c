/*
 * array.c
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <errcode.h>
#include <array.h>

void array_enable_lock(array *arr) {
	assert(arr != NULL);
	WRITE_LOCK(arr);
	arr->enable_lock = 1;
	WRITE_UNLOCK(arr);
}

void array_disable_lock(array *arr) {
	assert(arr != NULL);
	WRITE_LOCK(arr);
	arr->enable_lock = 0;
	WRITE_UNLOCK(arr);
}

size_t array_calc_capacity(size_t cap) {
	size_t capacity = 1;
	while(capacity < cap) {
		capacity <<= 1;
	}

	return capacity;
}

array *array_create_capacity(size_t capacity) {
	array *arr = (array *)malloc(sizeof(array));

	assert(arr != NULL);
	capacity = array_calc_capacity(capacity);
	arr->set = (array_node *)malloc(sizeof(array_node) * capacity);
	assert(NULL != arr->set);
	RWLOCK_INIT(arr);
	arr->size = 0;
	arr->capacity = capacity;
	arr->enable_lock = 1;

	return arr;
}

array *array_create() {
	return array_create_capacity(ARRAY_DEFAULT_CAPACITY);
}

size_t array_capacity(const array *arr) {
	return arr->capacity;
}

size_t array_size(const array *arr) {
	return arr->size;
}

int array_add(array *arr, void *object) {
	array_node *node;
	WRITE_LOCK(arr);
	if(arr->size >= arr->capacity) {
		int capacity = array_calc_capacity(arr->size + 1);
		array_node *new_set = (array_node *) realloc(arr->set, capacity * sizeof(array_node));
		if(NULL == new_set) {
			return ERROR_FAILD;
		}
		arr->set = new_set;
		arr->capacity = capacity;
	}
	node = &arr->set[arr->size];
	SPIN_INIT(node);
	node->value = object;
	++ arr->size;
	WRITE_UNLOCK(arr);

	return ERROR_SUCCESS;
}

void *array_remove(array *arr, size_t index) {
	void *value = NULL;
	if(array_size(arr) <= index)
		return value;

	WRITE_LOCK(arr);
	value = arr->set[index].value;
	arr->set[index].value = arr->set[arr->size-1].value;
	SPIN_DESTROY(&arr->set[arr->size-1]);
	arr->set[arr->size-1].value = NULL;
	-- arr->size;
	WRITE_UNLOCK(arr);

	return value;
}
/* 将数组容量修改为指定大小--即使不成功也有可能删除数据 */
int array_resize(array *arr, size_t size) {
	if(size <= 0) {
		return ERROR_FAILD;
	}

	if(array_capacity(arr) == size)
		return ERROR_SUCCESS;

	WRITE_LOCK(arr);
	if(size < arr->size){
		for(int i=size; i<arr->size; ++i) {
			array_node *node = &arr->set[i];
			if(node->value) {
				free(node->value);
				node->value = NULL;
			}
			SPIN_DESTROY(node);
		}
		arr->size = size;
	}

	array_node *new_set = (array_node *) realloc(arr->set, size);
	if(NULL == new_set) {
		return ERROR_FAILD;
	}
	arr->set = new_set;
	arr->capacity = size;
	WRITE_UNLOCK(arr);

	return ERROR_SUCCESS;
}

/* 严格紧凑数组 */
int array_strict_compact(array *arr) {
	return array_resize(arr, array_size(arr));
}

/* 紧凑数组 */
int array_compact(array *arr) {
	return array_resize(arr, array_calc_capacity(array_size(arr)));
}

/* 该函数需要被宏ARRAY_INDEX_LOCK和ARRAY_INDEX_UNLOCK包围 */
void *array_index(array *arr, size_t index) {
	if(arr->size <= index)
		return NULL;

	return arr->set[index].value;
}

void array_clear(array *arr) {
	WRITE_LOCK(arr);
	for(size_t i=0; i<arr->size; ++i) {
		array_node *node = &arr->set[i];
		if(node->value) {
			free(node->value);
			node->value = NULL;
		}
		SPIN_DESTROY(node);
	}
	arr->size = 0;
	WRITE_UNLOCK(arr);
}
/* 非线程安全 */
void array_destroy(array **arr) {
	if(arr == NULL || *arr==NULL)
		return;

	array_clear(*arr);
	RWLOCK_DESTROY(*arr);
	free((*arr)->set);
	free(*arr);
	*arr = NULL;
}

/* byte array */
byte_array *byte_array_create_buffer(char *buf, size_t size, size_t capacity) {
	assert(buf != NULL);
	byte_array *arr = (byte_array *)malloc(sizeof(byte_array));

	if(arr) {
		arr->block = buf;
		arr->size = size;
		arr->capacity = capacity;
	}

	return arr;
}

byte_array *byte_array_create_capacity(size_t capacity) {
	capacity = array_calc_capacity(capacity);
	char *block = (char *)malloc(capacity);
	if(block) {
		return byte_array_create_buffer(block, 0, capacity);
	}

	return NULL;
}

byte_array *byte_array_create() {
	return byte_array_create_capacity(ARRAY_DEFAULT_CAPACITY);
}

byte_array *byte_array_dup(byte_array *arr) {
	byte_array *new_arr = NULL;
	assert(arr != NULL);
	new_arr = byte_array_create_capacity(byte_array_capacity(arr));
	if(new_arr && ERROR_SUCCESS == byte_array_add(new_arr,
			byte_array_get_bytes(arr),
			byte_array_size(arr))) {
		return new_arr;
	}

	byte_array_destroy(&new_arr);

	return NULL;
}

size_t byte_array_capacity(const byte_array *arr) {
	return arr->capacity;
}

size_t byte_array_size(const byte_array *arr) {
	return arr->size;
}

int byte_array_add(byte_array *arr, const char *obj, size_t size) {
	size_t new_size = arr->size + size;
	if(new_size > arr->capacity) {
		size_t capacity = array_calc_capacity(new_size);
		char *new_block = (char *) realloc(arr->block, capacity);
		if(NULL == new_block) {
			return ERROR_FAILD;
		}
		arr->block = new_block;
		arr->capacity = capacity;
	}
	memcpy(arr->block + arr->size, obj, size);
	arr->size = new_size;

	return ERROR_SUCCESS;
}

char *byte_array_offset(byte_array *arr, int size) {
	assert(arr != NULL);
	int new_size = arr->size + size;
	if(new_size < 0)
		return NULL;

	if(new_size > arr->capacity) {
		size_t capacity = array_calc_capacity(new_size);
		char *new_block = (char *) realloc(arr->block, capacity);
		if(NULL == new_block) {
			return NULL;
		}
		arr->block = new_block;
		arr->capacity = capacity;
	}
	char *cur = arr->block + arr->size;
	arr->size = new_size;

	return cur;
}

int byte_array_assgin(byte_array *arr, const char *obj, size_t size) {
	if(arr->capacity < size) {
		int cap = array_calc_capacity(size);
		char *block = (char *)malloc(cap);
		if(block) {
			free(arr->block);
			arr->block = block;
			arr->capacity = cap;
		} else {
			return ERROR_FAILD;
		}
	}

	memcpy(arr->block, obj, size);
	arr->size = size;

	return ERROR_SUCCESS;
}

/* 返回操作结果 */
int byte_array_remove(byte_array *arr, size_t begin, size_t end) {
	if(begin > end || arr->size <= end) {
		return ERROR_FAILD;
	}

	if(arr->size-1== end) {
		arr->size = begin;
	} else {
		memcpy(arr->block + begin, arr->block + end + 1, arr->size - 1 - end);
		arr->size -= end - begin + 1;
	}

	return ERROR_SUCCESS;
}
/* 将数组容量修改为指定大小--即使不成功也有可能删除数据 */
int byte_array_resize(byte_array *arr, size_t size) {
	if(size <= 0) {
		return ERROR_FAILD;
	}

	if(byte_array_capacity(arr) == size)
		return ERROR_SUCCESS;

	if(size < arr->size){
		arr->size = size;
	}

	char *new_block = (char *) realloc(arr->block, size);
	if(NULL == new_block) {
		return ERROR_FAILD;
	}
	arr->block = new_block;
	arr->capacity = size;

	return ERROR_SUCCESS;
}

/* 严格紧凑数组 */
int byte_array_strict_compact(byte_array *arr) {
	return byte_array_resize(arr, byte_array_size(arr));
}

/* 紧凑数组 */
int byte_array_compact(byte_array *arr) {
	return byte_array_resize(arr, array_calc_capacity(byte_array_size(arr)));
}

char *byte_array_get_bytes(byte_array *arr) {
	return arr->block;
}

void byte_array_clear(byte_array *arr) {
	arr->size = 0;
}

void byte_array_swap(byte_array *arr1, byte_array *arr2) {
	byte_array tmp;
	tmp = *arr1;
	*arr1 = *arr2;
	*arr2 = tmp;
}

/* 非线程安全 */
void byte_array_destroy(byte_array **arr) {
	if(arr == NULL || *arr==NULL)
		return;

	byte_array_clear(*arr);
	free((*arr)->block);
	free(*arr);
	*arr = NULL;
}
