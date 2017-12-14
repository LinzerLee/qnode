/*
 * thread.c
 *
 *  Created on: 2017年10月18日
 *      Author: linzer
 */

#include <unistd.h>
#include <sys/syscall.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <define.h>
#include <timestamp.h>
#include <atomic.h>
#include <errcode.h>
#include <thread.h>

typedef struct thread{
	thread_state state;
	thread_type type;
	pid_t tid;
	pthread_t  pthreadId;
	char name[MAX_THREAD_NAME+1];
	thread_entry entry;
	int handle;
	spinlock lock;
} thread;

typedef struct threadpool{
	ARRAY threads;
	threadpool_limit limit;
	spinlock lock;
	bound_block_queue *queue;
	int worker;
	int service;
	atomic_t running;			/* bool */
} threadpool;

typedef struct threadpool_task{
	task_routine routine;
	task_done done;
	void *input;
	void *result;
	int errcode;
	queue_node node;
} threadpool_task;

static atomic_t g_threadCounter = { 0 }; /* 该原子变量只是用来生成线程默认名称 */
__thread static int t_cachedTid = 0;
__thread static char t_tidString[32];
__thread static const char* t_threadName = "unknown";
__thread static thread *t_thread = NULL;
static int g_mainThreadid = 0;
static atomic_t g_genThreadID = { 0 };

/* OSX 废弃了syscall接口，gettid始终返回-1 */
static inline int gettid() {
#ifdef __linux__
	return (pid_t)syscall(SYS_gettid);
#else
	return atomic_inc(&g_genThreadID);
#endif
}

static inline void cache_tid() {
	if (t_cachedTid == 0) {
		t_cachedTid = gettid();
		snprintf(t_tidString, sizeof(t_tidString), "%5d ", t_cachedTid);
		// printf("g_cachedTid : %ul\n", (unsigned int)g_cachedTid);
	}
}

inline int thread_current_id() {
	if (unlikely(t_cachedTid == 0)) {
		cache_tid();
    }

    return t_cachedTid;
}

const char *thread_current_idstring() {
	return t_tidString;
}

inline const char *thread_current_name() {
	return t_threadName;
}

inline bool thread_is_main() {
	if (unlikely(g_mainThreadid == 0)) {
		g_mainThreadid = getpid();
	}

	return thread_current_id() == g_mainThreadid;
}

static inline void thread_reset_info(thread *t) {
	CHECK_VAILD_PTR(t);
	t->state = nostart;
	t->entry.arg = NULL;
	t->entry.callback = NULL;
	t->pthreadId = INVAILD_THREAD_ID;
	t->tid = INVAILD_THREAD_ID;
	t->handle = INVAILD_THREAD_HANDLE;
	t->type = INVAILD_THREAD_TYPE;
	t->name[0] = '\0';
}

void thread_init(thread *t, thread_callback callback,
		void *arg, const char *tname, thread_type type, HANDLE handle) {
	CHECK_VAILD_PTR(t);
	CHECK(handle != INVAILD_THREAD_HANDLE);
	CHECK(type>MIN_THREAD_TYPE && type<MAX_THREAD_TYPE);
	if(THREAD_MAIN != type) {
		CHECK(callback != NULL);
	}

	if(t) {
		SPIN_INIT(t);
		thread_reset_info(t);
		t->entry.callback = callback;
		t->entry.arg = arg;
		t->type = type;
		t->handle = handle;
		if(tname) {
			int len = strlen(tname);
			if(len > MAX_THREAD_NAME)
				len = MAX_THREAD_NAME;
			memcpy(t->name, tname, len);
			t->name[len] = '\0';
		}
	}
}

static inline void inner_set_defaultname(thread *t) {
  int num = atomic_inc(&g_threadCounter);

  if (t->name[0] == '\0') {
    snprintf(t->name, sizeof t->name, "Thread%d", num);
  }
}

static void thread_boost_init(thread *t) {
	assert(NULL != t);
	pid_t tid = thread_current_id();
	t->tid = tid;

	t_thread = t;
	t_threadName = t->name;
}

static void *thread_boost(void *data) {
	char buf[MAX_THREAD_NAME+1];
	thread *t = (thread *)data;
	thread_boost_init(t);
	// 防止线程状态的竟态条件--可能会是stoping状态
	SPIN_LOCK(t);
	if(t->state == starting) {
		t->state = started;
	}
	SPIN_UNLOCK(t);
	if(t->state == started)
		t->entry.callback(t->entry.arg);
	/* 线程结束 */
	SPIN_LOCK(t);
	t->state = stoped;
	SPIN_UNLOCK(t);
	int len = sizeof buf;
	snprintf(buf, len, "%s finished", t->name);
	len = MIN(strlen(buf), sizeof t->name);
	SPIN_LOCK(t);
	memcpy(t->name, buf, len);
	t->name[len] = '\0';
	SPIN_UNLOCK(t);
	return NULL;
}

int thread_start(thread *t) {
	assert(t->state == nostart);
	int errcode = ERROR_SUCCESS;
	// 防止线程状态的竟态条件--可能会是stoping状态
	// 保证不会出现stoping->starting的切换
	SPIN_LOCK(t);
	if(t->state == nostart) {
		t->state = starting;
		inner_set_defaultname(t);
		if (pthread_create(&t->pthreadId, NULL, thread_boost, t)) {
			t->state = nostart;
			errcode = ERROR_THREAD_START_FAILD;
			// LOG_SYSFATAL << "Failed in pthread_create";
		}
	}
	SPIN_UNLOCK(t);
	return errcode;
}

void thread_stop(thread *t) {
	/* 只停止starting和started状态的线程 */
	SPIN_LOCK(t);
	if(stoped != t->state && joined != t->state) {
		if(nostart == t->state)
			t->state = joined;
		else
			t->state = stopping;
	}
	SPIN_UNLOCK(t);
}

void thread_join(thread *t) {
	assert(nostart != t->state);
	assert(joined != t->state);
	pthread_join(t->pthreadId, NULL);
	SPIN_LOCK(t);
	t->state = joined;
	SPIN_UNLOCK(t);
}

int thread_yield() {
	int errcode = sched_yield();
	errcode = errcode != 0 ? ERROR_FAILD : errcode ;

	return errcode;
}

void thread_release(thread *t) {
	SPIN_LOCK(t);
	if (nostart != t->state && joined != t->state) {
		pthread_detach(t->pthreadId);
	}
	thread_reset_info(t);
	t->state = joined;
	SPIN_UNLOCK(t);
}

thread_state thread_get_state(thread *t) {
	CHECK_VAILD_PTR(t);
	return t->state;
}

void thread_sleep(int64_t ms) {
  struct timespec ts = { 0, 0 };
  ts.tv_sec = (time_t) (ms / MICRO_SECOND_PER_SECOND);
  ts.tv_nsec = (long) (ms % MICRO_SECOND_PER_SECOND * 1000);
  nanosleep(&ts, NULL);
}

void thread_once(thread_once_t *guard, once_callback callback) {
	if (pthread_once(guard, callback))
		abort();
}

thread *thread_self() {
	return t_thread;
}

#define INVAILD_LIMIT	-1
#define NO_LIMIT			-2

/* thread limit */
void threadpool_limit_init(threadpool_limit *limit) {
	assert(limit != NULL);
	limit->worker = INVAILD_LIMIT;
	limit->service = NO_LIMIT;
	limit->task = INVAILD_LIMIT;
}

void threadpool_limit_worker(threadpool_limit *limit, int num) {
	assert(limit != NULL);
	assert(num > 0);
	limit->worker = num;
}

void threadpool_limit_service(threadpool_limit *limit, int num) {
	assert(limit != NULL);
	IGNORE(num);
}

void threadpool_limit_task(threadpool_limit *limit, int num) {
	assert(limit != NULL);
	assert(num > 0);
	limit->task = num;
}


#define DEFAULT_SERVICE_THREAD_NUM	NO_LIMIT
#define DEFAULT_WORKER_THREAD_NUM	4
#define DEFAULT_TASKQUEUE_NUM		1024

threadpool_task *threadpool_task_create(task_routine routine, void *input, task_done done) {
	threadpool_task *task = (threadpool_task *)malloc(sizeof(threadpool_task));
	if(task) {
		task->routine = routine;
		task->done = done;
		task->input = input;
		task->errcode = ERROR_SUCCESS;
		task->result = NULL;
	}

	return task;
}

static threadpool *P = NULL;
static threadpool_limit DEFAULT_LIMIT = { DEFAULT_SERVICE_THREAD_NUM, DEFAULT_WORKER_THREAD_NUM, DEFAULT_TASKQUEUE_NUM };
/* thread pool */
int threadpool_init(threadpool_limit *limit) {
	int errcode = ERROR_FAILD;
	MALLOC_DEF(pool, threadpool);
	if(!TEST_VAILD_PTR(pool))
		return errcode;

	if(TEST_VAILD_PTR(limit)) {
		pool->limit = *limit;
	} else {
		pool->limit = DEFAULT_LIMIT;
	}

	pool->limit.service = DEFAULT_SERVICE_THREAD_NUM;

	if(pool->limit.worker <= 0) {
#ifdef __linux__
		pool->limit.worker = get_nprocs();
#endif
		pool->limit.worker = DEFAULT_WORKER_THREAD_NUM;
	}

	if(pool->limit.task <= 0)
		pool->limit.task = DEFAULT_TASKQUEUE_NUM;

	pool->service = 0;
	pool->worker = 0;
	SPIN_INIT(pool);
	ARRAY_NEW(pool->threads);
	if(TEST_VAILD_PTR(pool->threads)) {
		pool->queue = bound_block_queue_create(pool->limit.task);
		if(TEST_VAILD_PTR(pool->queue)) {
			ATOMIC_TRUE(pool->running);
			P = pool;

			/* 将main线程添加进thread pool */
			thread *t = ARRAY_OFFSET(P->threads, thread, 1);
			if(TEST_VAILD_PTR(t)) {
				HANDLE handle = ARRAY_SIZE(P->threads, thread) - 1;
				thread_init(t, NULL, NULL, "dispatch", THREAD_MAIN, handle);
			}

			errcode = ERROR_SUCCESS;
			return errcode;
		}

		ARRAY_DESTROY(pool->threads);
	}

	FREE(pool);

	return errcode;
}

void free_task_node(queue_node *node) {
	if(node) {
		threadpool_task *task = DATA(node, threadpool_task, node);
		assert(task != NULL);
		free(task);
	}
}

void threadpool_release() {
	if(TEST_VAILD_PTR(P)) {
		CHECK(!ATOMIC_TEST(P->running));
		SPIN_LOCK(P);
		ARRAY_DESTROY(P->threads);
		bound_block_queue_destroy(&P->queue, free_task_node);
		SPIN_UNLOCK(P);
		SPIN_DESTROY(P);
		FREE(P);
	}
}

thread *threadpool_handle_to_thread(int handle) {
	CHECK_VAILD_PTR(P);
	thread *t = NULL;
	SPIN_LOCK(P);
	if(handle > INVAILD_THREAD_HANDLE &&
			handle < ARRAY_SIZE(P->threads, thread)) {
		t = ARRAY_AT_PTR(P->threads, thread, handle);
	}
	SPIN_UNLOCK(P);
	return t;
}

void threadpool_adjust_limit(threadpool_limit *limit) {
	CHECK_VAILD_PTR(P);
	SPIN_LOCK(P);
	if(limit->worker > 0)
		P->limit.worker = limit->worker;

	if(limit->task > 0) {
		P->limit.task = limit->task;
		bound_block_queue_set_capacity(P->queue, P->limit.task);
	}
	SPIN_UNLOCK(P);
}

int threadpool_size() {
	CHECK_VAILD_PTR(P);
	return P->worker + P->service + 1;
}

static inline bool check_can_create_worker() {
	CHECK_VAILD_PTR(P);
	if(P->worker < P->limit.worker)
		return true;
	else
		return false;

}

int threadpool_boost_thread(thread_callback callback, void *arg, const char *tname, thread_type type) {
	CHECK_VAILD_PTR(P);
	int handle = INVAILD_THREAD_HANDLE;
	thread *t = NULL;

	SPIN_LOCK(P);
	if(check_can_create_worker()) {
		/* 优先复用旧的空间 */
		ARRAY_FOREACH(ptr, P->threads, thread) {
			if(ptr) {
				if(ptr->type == THREAD_WORKER && ptr->state == joined) {
					t = ptr;
					thread_release(t);
					handle = ARRAY_INDEX(ptr, P->threads, thread);
				}
			}
		}
		/* 开辟新的空间 */
		if(!t) {
			t = ARRAY_OFFSET(P->threads, thread, 1);
			if(t) {
				handle = ARRAY_SIZE(P->threads, thread) - 1;
			}
		}
		/* 初始化 */
		if(t) {
			thread_init(t, callback, arg, tname, type, handle);
			if(type == THREAD_SERVICE)
				++ P->service;
			else if(type == THREAD_WORKER)
				++ P->worker;
		 }
	}

	SPIN_UNLOCK(P);

	if(t) {
		if(ERROR_SUCCESS != thread_start(t)) {
			SPIN_LOCK(P);
			if(type == THREAD_SERVICE)
				-- P->service;
			else if(type == THREAD_WORKER)
				-- P->worker;
			SPIN_UNLOCK(P);
			handle = INVAILD_THREAD_HANDLE;
		}
	}

	return handle;
}

int threadpool_apply_service(thread_callback callback, void *arg, const char *tname) {
	CHECK_VAILD_PTR(P);
	return threadpool_boost_thread(callback, arg, tname, THREAD_SERVICE);
}

void worker_routine(void *input) {
	threadpool *pool = (threadpool *)input;
	thread *self = thread_self();
	assert(pool != NULL);
	assert(self != NULL);
	queue_node *node = NULL;

	while (self->state == started) {
		node = bound_block_queue_pop(pool->queue);
		if(node) {
			threadpool_task *task = DATA(node, threadpool_task, node);
			if (TEST_VAILD_PTR(task->input)) {
				task->result = task->routine(task->input, &task->errcode);
				if (TEST_VAILD_PTR(task->done)) {
					task->done(task->result, task->errcode);
				}
			}
			// 优化：使用task链
			FREE(task);
		}
	}
}

int threadpool_submit(threadpool_task *task) {
	CHECK_VAILD_PTR(P);
	if(P->worker < P->limit.worker) {
		char buf[MAX_THREAD_NAME + 1];
		snprintf(buf, sizeof buf, "worker%d", P->worker + 1);
		(void)threadpool_boost_thread(worker_routine, P, buf, THREAD_WORKER);
	}
	if(bound_block_queue_push(P->queue, &task->node))
		return ERROR_SUCCESS;
	else
		return ERROR_FAILD;
}

void threadpool_stop_thread(int handle) {
	CHECK_VAILD_PTR(P);
	CHECK_VAILD_HANDLE(handle);
	thread *t = threadpool_handle_to_thread(handle);
	thread_stop(t);
}

void threadpool_stop() {
	CHECK_VAILD_PTR(P);
	SPIN_LOCK(P);
	ARRAY_FOREACH(t, P->threads, thread) {
		if(t) {
			if(t->type==THREAD_SERVICE || t->type == THREAD_WORKER) {
				thread_stop(t);
				// printf("stop thread %s\n", t->name);
			}
		}
	}
	SPIN_UNLOCK(P);
	bound_block_queue_finish(P->queue);

	ARRAY_FOREACH(t, P->threads, thread) {
		if(t) {
			if(t->type==THREAD_SERVICE || t->type == THREAD_WORKER) {
				if(t->state == stopping || t->state == stoped) {
					thread_join(t);
					// printf("join thread %s\n", t->name);
					SPIN_LOCK(P);
					if(t->type == THREAD_SERVICE)
						-- P->service;
					else if(t->type == THREAD_WORKER)
						-- P->worker;
					SPIN_UNLOCK(P);
				}
			}
		}
	}
	ATOMIC_FALSE(P->running);
}
