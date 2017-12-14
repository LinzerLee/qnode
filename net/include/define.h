/*
 * define.h
 *
 *  Created on: 2017年10月17日
 *      Author: linzer
 */

#ifndef __QNODE_DEFINE_H__
#define __QNODE_DEFINE_H__

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define IN
#define OUT
#define MOV

#define IGNORE(var)	(void)var
#define DESTRUCTOR(declaration) __attribute__((destructor)) declaration
#define UNUSED(declaration)     __attribute__((unused)) declaration
#define FORWARD_DECLAR(type)		\
	struct type; 				\
	typedef struct type type;

#define DATA(ptr, type, field)                                         \
  ((type *) ((char *) (ptr) - offsetof(type, field)))

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

#ifndef MIN
#define MIN(a, b) 			((a) <= (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) 			((a) > (b) ? (a) : (b))
#endif
#define SIZE(arr)			(sizeof(arr) / sizeof((arr)[0]))
#define LENGTH(str)			strlen(str)

#define TEST_SUCCESS(exp)			\
	((exp)==ERROR_SUCCESS)
#define TEST_FAILD(exp)				\
	((exp)==ERROR_FAILD)

#define CHECK_SUCCESS(exp)			\
	assert(TEST_SUCCESS(exp))
#define CHECK_FAILD(exp)				\
	assert(TEST_SUCCESS(exp))

#define INVAILD_HANDLE -1
#define INVAILD_FD -1
#define INVAILD_SOCKET -1

#define TEST_VAILD_FD(fd)						((fd) >= 0)
#define TEST_VAILD_SOCKET(sockfd)				((sockfd) >= 0)
#define TEST_VAILD_HANDLE(handle)				((handle) >=0)
#define TEST_VAILD_SERVICE_HANDLE(handle)		(handle != INVAILD_SERVICE_HANDLE)
#define TEST_VAILD_PTR(ptr)						((ptr) != NULL)
#define TEST_EQUAL(a, b)							((a) == (b))

#define CHECK(exp)								assert(exp)
#define CHECK_VAILD_FD(fd)						assert(TEST_VAILD_FD(fd))
#define CHECK_VAILD_SOCKET(sockfd)				assert(TEST_VAILD_SOCKET(sockfd))
#define CHECK_VAILD_HANDLE(handle)				assert(TEST_VAILD_HANDLE(handle))
#define CHECK_VAILD_SERVICE_HANDLE(handle)		assert(TEST_VAILD_SERVICE_HANDLE(handle))
#define CHECK_VAILD_PTR(ptr)						assert(TEST_VAILD_PTR(ptr))
#define CHECK_EQUAL(a, b)						assert(TEST_EQUAL(a, b))

#define OFFSETOF(t, f)				__builtin_offsetof(t, f)
#define STATIC_ASSERT(COND, MSG) 	typedef char static_assertion_##MSG[(COND)?1:-1]

#ifndef bool
typedef int bool;
#endif

#ifndef true
#define true			1
#endif

#ifndef false
#define false		0
#endif

typedef int HANDLE;

#define STRUCT_ZERO(ptr)			\
		memset(ptr, 0, sizeof(*ptr))

#define MALLOC_SIZE(ptr, size)		\
	ptr = malloc(size)

#define MALLOC(ptr, type)			\
	ptr = (type *)malloc(sizeof(type))

#define MALLOC_DEF(ptr, type)			\
	type *ptr = (type *)malloc(sizeof(type))

#define NEW(ptr, type)			\
	ptr = (type *)malloc(sizeof(type))

#define FREE(ptr)					\
	free(ptr);						\
	(ptr)=NULL;

#define ZERO(ptr)				\
	memset(ptr, 0, sizeof(ptr))

#define NUL(ptr)				\
	(ptr = NULL)

#ifndef TRUE
#define TRUE(var)				\
	(var = true)
#endif

#ifndef FALSE
#define FALSE(var)				\
	(var = false)
#endif

#define ATOMIC_TRUE(a)			\
	(atomic_set(&a, true))

#define ATOMIC_FALSE(a)			\
	(atomic_set(&a, false))

#define ATOMIC_TRUE(a)			\
	(atomic_set(&a, true))

#define ATOMIC_TEST(a)			\
	(atomic_get(&a))

#define ABORT	abort();

typedef int STAGE_TYPE;

#define STAGE_EQUAL		0
#define STAGE_AFTER		1
#define STAGE_SINCE		2
#define STAGE_BEFORE		3
#define STAGE_UNTIL		4

#define TEST_STAGE(state, stage)	\
		((state) == (stage))

#define TEST_STAGE_AFTER(state, stage)	\
		((state) > (stage))

#define TEST_STAGE_SINCE(state, stage)	\
		((state) >= (stage))

#define TEST_STAGE_BEFORE(state, stage)	\
		((state) < (stage))

#define TEST_STAGE_UNTIL(state, stage)	\
		((state) <= (stage))

#define NODE_ID(harbor_id)					((harbor_id & 0xFFFFFFFF) >> 16)
#define SERVICE_ID(harbor_id)			(harbor_id & 0x0000FFFF)
#define HARBOR_ID(node_id, service_id)	((node_id & 0xFFFFFFFF) << 16 | (service_id & 0x0000FFFF))

typedef struct weak_ptr{
	void *ptr;
} weak_ptr;
#endif /* __QNODE_DEFINE_H__ */
