/*
 * thread.h
 *
 *  Created on: 2017年10月18日
 *      Author: linzer
 */

#ifndef __QNODE_THREAD_H__
#define __QNODE_THREAD_H__

#include <pthread.h>
#include <sys/types.h>

#include <define.h>
#include <spinlock.h>
#include <array.h>
#include <queue.h>

typedef void (*thread_callback)(void *);
typedef void (*once_callback)(void);

typedef enum {
	nostart,		/* 线程尚未启动--初始化后才有的状态 */
	starting,	/* 线程已经启动 */
	started,		/* 线程循环时只能是这个状态 */
	busying,
	idling,
	stopping,	/* 请求停止 */
	stoped,		/* 停止运行 */
	joined,		/* 待初始化 */
	MAX_THREAD_STATE
} thread_state;

typedef enum {
	MIN_THREAD_TYPE,
	THREAD_MAIN,
	THREAD_SERVICE,
	THREAD_WORKER,
	MAX_THREAD_TYPE
} thread_type;

#define INVAILD_THREAD_HANDLE	INVAILD_HANDLE
#define INVAILD_THREAD_TYPE		MIN_THREAD_TYPE

typedef pthread_once_t thread_once_t;
#define THREAD_ONCE_INIT PTHREAD_ONCE_INIT

#define MAX_THREAD_NAME	31
#define INVAILD_THREAD_ID 0

typedef struct {
	thread_callback callback;
	void *arg;
} thread_entry;

FORWARD_DECLAR(thread)

/* 外部API使用thread handle，内部API使用thread */
void thread_init(thread *t, thread_callback callback,
		void *arg, const char *tname, thread_type type, HANDLE handle);
int thread_start(thread *t);
void thread_stop(thread *t);
void thread_join(thread *t);
int thread_yield();
void thread_release(thread *t);
thread_state thread_get_state(thread *t);
void thread_sleep(int64_t ms);
int thread_current_id();
const char *thread_current_idstring();
const char *thread_current_name();
bool thread_is_main();
void thread_once(thread_once_t *guard, once_callback callback);
thread *thread_self();

typedef struct threadpool_limit{
	int worker;
	int service;
	int task;
} threadpool_limit;

void threadpool_limit_init(threadpool_limit *limit);
void threadpool_limit_worker(threadpool_limit *limit, int num);
void threadpool_limit_service(threadpool_limit *limit, int num);
void threadpool_limit_task(threadpool_limit *limit, int num);

typedef void *(*task_routine)(void *input, int *errcode);
typedef void (*task_done)(void *result, int errcode);

FORWARD_DECLAR(threadpool_task)
threadpool_task *threadpool_task_create(task_routine routine, void *input, task_done done);

FORWARD_DECLAR(threadpool)
int threadpool_init(threadpool_limit *limit);
void threadpool_release();
void threadpool_adjust_limit(threadpool_limit *limit);
int threadpool_size();
int threadpool_boost_thread(thread_callback callback, void *arg, const char *tname, thread_type type);
int threadpool_apply_service(thread_callback callback, void *arg, const char *tname);
thread *threadpool_handle_to_thread(int handle);
int threadpool_submit(threadpool_task *task);
void threadpool_stop_thread(int handle);
void threadpool_stop();
#endif /* __QNODE_THREAD_H__ */
