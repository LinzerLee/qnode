/*
 * stringpiece.h
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */

#ifndef __QNODE_STRINGPIECE_H__
#define __QNODE_STRINGPIECE_H__

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <errcode.h>
#include <array.h>

typedef struct {
	byte_array *arr;
} stringpiece;

static inline int stringpiece_add(stringpiece *strpie, char ch) {
	assert(strpie != NULL);
	int ret = ERROR_SUCCESS;
	byte_array_offset(strpie->arr, -1);
	ret = byte_array_add(strpie->arr, &ch, sizeof(char));
	ch = '\0';
	ret |= byte_array_add(strpie->arr, &ch, sizeof(char));
	return ret;
}

static inline int stringpiece_append(stringpiece *strpie, const char *str) {
	int errcode = ERROR_SUCCESS;

	assert(strpie != NULL);
	if(str != NULL) {
		int end = byte_array_size(strpie->arr) - 1;
		errcode = byte_array_remove(strpie->arr, end, end);
		if(ERROR_SUCCESS == errcode) {
			errcode = byte_array_add(strpie->arr, str, strlen(str) + 1);
		}
	}

	return errcode;
}

static inline int stringpiece_init(stringpiece *strpie) {
	assert(strpie != NULL);
	strpie->arr = byte_array_create();
	char ch = '\0';
	return byte_array_add(strpie->arr, &ch, sizeof(char));
}

static inline int stringpiece_init_cstring(stringpiece *strpie, const char *str) {
	assert(strpie != NULL);
	if(str != NULL) {
		strpie->arr = byte_array_create();
		return byte_array_add(strpie->arr, str, strlen(str) + 1);
	} else {
		return stringpiece_init(strpie);
	}
}

static inline char * stringpiece_data(const stringpiece *strpie) {
	return (char *)byte_array_get_bytes(strpie->arr);
}

static inline const char * stringpiece_to_cstring(const stringpiece *strpie) {
	return (const char *)stringpiece_data(strpie);
}

static inline int stringpiece_init_strpie(stringpiece *strpie, const stringpiece *str) {
	assert(strpie != NULL);
	return stringpiece_init_cstring(strpie, stringpiece_to_cstring(str));
}

static inline int
stringpiece_init_buffer(stringpiece *strpie, char *buf, size_t size) {
	assert(strpie != NULL);
	assert(buf != NULL);
	assert(size > 0);

	strpie->arr = byte_array_create_capacity(size + 1);

	if(strpie->arr) {
		byte_array_add(strpie->arr, buf, size);
		char ch = '\0';
		byte_array_add(strpie->arr, &ch, sizeof ch);

		return ERROR_SUCCESS;
	} else {
		return ERROR_FAILD;
	}
}

static inline int stringpiece_assgin(stringpiece *strpie, const char *str) {
	assert(strpie != NULL);
	return byte_array_assgin(strpie->arr, str, strlen(str) + 1);
}

static inline size_t stringpiece_size(const stringpiece *strpie) {
	return byte_array_size(strpie->arr) - 1;
}

static inline int stringpiece_empty(const stringpiece *strpie) {
	return 0 == stringpiece_size(strpie);
}

static inline char stringpiece_at(stringpiece *strpie, size_t index) {
	assert(index+1 <= stringpiece_size(strpie));

	return byte_array_get_bytes(strpie->arr)[index];
}

static inline int stringpiece_remove(stringpiece *strpie, size_t pos, size_t size) {
	size_t old_size = stringpiece_size(strpie);
	size_t start = pos, end = pos + size - 1;

	if(size <= 0)
		return ERROR_SUCCESS;

	if(start+1 > old_size)
		return ERROR_SUCCESS;

	if(end+1 > old_size)
		end = old_size - 1;

	return byte_array_remove(strpie->arr, start, end);
}

static inline void stringpiece_clear(stringpiece *strpie) {
	byte_array_clear(strpie->arr);
	char ch = '\0';
	byte_array_add(strpie->arr, &ch, 1);
}

static inline int stringpiece_remove_prefix(stringpiece *strpie, int n) {
	return stringpiece_remove(strpie, 0, n);
}

static inline int stringpiece_remove_suffix(stringpiece *strpie, int n) {
	size_t old_size = stringpiece_size(strpie);
	return stringpiece_remove(strpie, old_size - n, n);
}

static inline void stringpiece_swap(stringpiece *strpie1, stringpiece *strpie2) {
	byte_array *tmp;
	tmp = strpie1->arr;
	strpie1->arr = strpie2->arr;
	strpie2->arr = tmp;
}

static inline int stringpiece_copy(stringpiece *dest, stringpiece *src) {
	assert(src != NULL);
	assert(dest != NULL);
	byte_array *new_arr = byte_array_dup(src->arr);

	if(new_arr) {
		byte_array_destroy(&dest->arr);
		dest->arr = new_arr;

		return ERROR_SUCCESS;
	}

	return ERROR_FAILD;
}

static inline stringpiece stringpiece_dup(stringpiece *strpie) {
	assert(strpie != NULL);
	stringpiece tmp;
	stringpiece_init_strpie(&tmp, strpie);

	return tmp;
}

static inline void stringpiece_release(stringpiece *strpie) {
	if(strpie) {
		byte_array_destroy(&strpie->arr);
	}
}

/* 字符串操作 */
#define STRINGPIECE_NOPOS	-1
static inline int stringpiece_find_char(stringpiece *strpie, char ch) {
	assert(strpie != NULL);
	const char *str = stringpiece_to_cstring(strpie);
	const char *cur = strchr(str, ch);
	int pos = 0;
	if(cur) {
		pos = cur - str;
	} else {
		pos = STRINGPIECE_NOPOS;
	}

	return pos;
}

static inline int stringpiece_rfind_char(stringpiece *strpie, char ch) {
	assert(strpie != NULL);
	const char *str = stringpiece_to_cstring(strpie);
	const char *cur = strrchr(str, ch);
	int pos = 0;
	if(cur) {
		pos = cur - str;
	} else {
		pos = STRINGPIECE_NOPOS;
	}

	return pos;
}

static inline int stringpiece_find_str(stringpiece *strpie, const char *substr) {
	assert(strpie != NULL);
	const char *str = stringpiece_to_cstring(strpie);
	const char *cur = strstr(str, substr);
	int pos = 0;
	if(cur) {
		pos = cur - str;
	} else {
		pos = STRINGPIECE_NOPOS;
	}

	return pos;
}

static inline stringpiece stringpiece_substr(stringpiece *strpie, size_t pos, size_t size) {
	assert(strpie != NULL);
	assert(pos + size <= stringpiece_size(strpie));
	stringpiece tmp;
	stringpiece_init_buffer(&tmp, stringpiece_data(strpie) + pos, size);

	return tmp;
}
#endif /* __QNODE_STRINGPIECE_H__ */
