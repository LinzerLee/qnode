/*
 * queue.c
 *
 *  Created on: 2017年10月19日
 *      Author: linzer
 */

#include <queue.h>

/* 私有宏. */
#define QUEUE_EMPTY(head)		DCLIST_EMPTY(head)
#define QUEUE_HEAD(head)			DCLIST_HEAD(head)
#define QUEUE_TAIL(head)			DCLIST_TAIL(head)
#define QUEUE_INIT(node)			DCLIST_INIT(node)
#define QUEUE_ADD(head, other) 	DCLIST_ADD(head, other)
#define QUEUE_REMOVE(node)		DCLIST_REMOVE(node)
#define QUEUE_MOVE(head, other)	DCLIST_MOVE(head, other)
#define QUEUE_SPLIT(head, split, other) 							\
			DCLIST_SPLIT(head, split, other)
#define QUEUE_INSERT_HEAD(head, node) 							\
			DCLIST_INSERT_HEAD(head, node)
#define QUEUE_INSERT_TAIL(head, node)							\
			DCLIST_INSERT_TAIL(head, node)

queue *queue_create() {
	queue *q = (queue *)malloc(sizeof(queue));
	assert(NULL != q);
	QUEUE_INIT(&q->queue_head);
	SPIN_INIT(q);
	q->size = 0;
	return q;
}

int queue_empty(const queue *q) {
	assert(q != NULL);
	return q->size == 0;
}

int queue_size(const queue *q) {
	assert(q != NULL);
	return q->size;
}

void queue_push(queue *q, queue_node *node) {
	assert(q != NULL);
	assert(NULL != node);
	SPIN_LOCK(q);
	QUEUE_INSERT_TAIL(&q->queue_head, node);
	++ q->size;
	SPIN_UNLOCK(q);
}

queue_node *queue_unpush(queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	SPIN_LOCK(q);
	if(!queue_empty(q)) {
		node = QUEUE_TAIL(&q->queue_head);
		QUEUE_REMOVE(node);
		-- q->size;
	}
	SPIN_UNLOCK(q);

	return node;
}

queue_node *queue_pop(queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	SPIN_LOCK(q);
	if(!queue_empty(q)) {
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		-- q->size;
	}
	SPIN_UNLOCK(q);

	return node;
}

queue_node *queue_front(queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	SPIN_LOCK(q);
	if(!queue_empty(q)) {
		node = QUEUE_HEAD(&q->queue_head);
	}
	SPIN_UNLOCK(q);

	return node;
}

queue_node *queue_back(queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	SPIN_LOCK(q);
	if(!queue_empty(q)) {
		node = QUEUE_TAIL(&q->queue_head);
	}
	SPIN_UNLOCK(q);

	return node;
}

void queue_clear(queue *q, queue_node_free_cb free_cb) {
	assert(q != NULL);
	queue_node *node = NULL;
	if(NULL == q)
		return;

	SPIN_LOCK(q);
	while(!queue_empty(q)) {
		node = queue_pop(q);
		free_cb(node);
	}

	q->size = 0;
	SPIN_UNLOCK(q);
}

/* 非线程安全 */
void queue_destroy(queue **q, queue_node_free_cb free_cb) {
	if(q==NULL || *q==NULL)
		return;
	queue_clear(*q, free_cb);
	SPIN_DESTROY(*q);

	free(*q);
	*q = NULL;
}

/* quick queue */
quick_queue *quick_queue_create() {
	quick_queue *q = (quick_queue *)malloc(sizeof(quick_queue));
	assert(NULL != q);
	atomic_ptr_set(&q->node, NULL);

	return q;
}

int quick_queue_empty(const quick_queue *q) {
	assert(q != NULL);
	return NULL == atomic_ptr_get(&q->node);
}

void quick_queue_push(quick_queue *q, queue_node *node) {
	assert(q != NULL);
	node = atomic_ptr_set(&q->node, node);
	if(node) {
		free(node);
	}
}

queue_node *quick_queue_pop(quick_queue *q) {
	assert(q != NULL);
	return atomic_ptr_set(&q->node, NULL);
}

queue_node *quick_queue_unpush(quick_queue *q) {
	assert(q != NULL);
	return quick_queue_pop(q);
}

void quick_queue_clear(quick_queue *q) {
	assert(q != NULL);
	queue_node *node = quick_queue_unpush(q);
	if(node) {
		free(node);
	}
}

/* 非线程安全 */
void quick_queue_destroy(quick_queue **q) {
	if(q==NULL || *q==NULL)
		return;

	quick_queue_clear(*q);

	free(*q);
	*q = NULL;
}

/* block queue */
block_queue *block_queue_create() {
	block_queue *q = (block_queue *)malloc(sizeof(block_queue));
	assert(NULL != q);
	QUEUE_INIT(&q->queue_head);
	MUTEX_INIT(q);
	condition_init(&q->cond, &q->lock);
	q->size = 0;
	q->finish = false;
	return q;
}

int block_queue_empty(const block_queue *q) {
	assert(q != NULL);
	return q->size == 0;
}

int block_queue_size(const block_queue *q) {
	assert(q != NULL);
	return q->size;
}

bool block_queue_push(block_queue *q, queue_node *node) {
	assert(q != NULL);
	assert(node != NULL);
	bool ret = true;
	MUTEX_LOCK(q);
	if(q->finish) {
		ret = false;
	} else {
		QUEUE_INSERT_TAIL(&q->queue_head, node);
		++ q->size;
		if(1 == q->size) {
			/* 优化:只在为空时才notify */
			condition_notify(&q->cond);
		}
	}
	MUTEX_UNLOCK(q);

	return ret;
}

queue_node *block_queue_pop(block_queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	MUTEX_LOCK(q);
	while(!q->finish && block_queue_empty(q)) {
		condition_wait(&q->cond);
	}
	assert(q->finish || !block_queue_empty(q));
	if(!block_queue_empty(q)) {
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		-- q->size;
	}
	MUTEX_UNLOCK(q);
	return node;
}

queue_node *block_queue_try_pop(block_queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	MUTEX_LOCK(q);
	if(!block_queue_empty(q)) {
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		-- q->size;
	}
	MUTEX_UNLOCK(q);
	return node;
}

void block_queue_finish(block_queue *q) {
	assert(q != NULL);
	MUTEX_LOCK(q);
	q->finish = true;
	MUTEX_UNLOCK(q);
}

void block_queue_clear(block_queue *q, queue_node_free_cb free_cb) {
	assert(q != NULL);
	queue_node *node = NULL;
	if(NULL == q)
		return;

	MUTEX_LOCK(q);
	while(!QUEUE_EMPTY(&q->queue_head)) {
		/* 不能用pop否则会阻塞 */
		// node = block_queue_pop(q);
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		free_cb(node);
	}
	q->size = 0;
	MUTEX_UNLOCK(q);
}

/* 非线程安全 */
void block_queue_destroy(block_queue **q, queue_node_free_cb free_cb) {
	if(q && *q) {
		block_queue_clear(*q, free_cb);
		MUTEX_DESTROY(*q);
		condition_destroy(&(*q)->cond);

		free(*q);
		*q = NULL;
	}
}

/* bound block queue */
bound_block_queue *bound_block_queue_create(int capacity) {
	bound_block_queue *q = (bound_block_queue *)malloc(sizeof(bound_block_queue));
	assert(NULL != q);
	QUEUE_INIT(&q->queue_head);
	MUTEX_INIT(q);
	condition_init(&q->no_full, &q->lock);
	condition_init(&q->no_empty, &q->lock);
	q->size = 0;
	q->capacity = capacity;
	q->finish = false;
	return q;
}

int bound_block_queue_empty(const bound_block_queue *q) {
	assert(q != NULL);
	return q->size == 0;
}

int bound_block_queue_full(const bound_block_queue *q) {
	assert(q != NULL);
	return q->size >= q->capacity;
}

int bound_block_queue_size(const bound_block_queue *q) {
	assert(q != NULL);
	return q->size;
}

int bound_block_queue_capacity(const bound_block_queue *q) {
	assert(q != NULL);
	return q->capacity;
}

bool bound_block_queue_push(bound_block_queue *q, queue_node *node) {
	assert(q != NULL);
	assert(node != NULL);
	int ret = true;
	MUTEX_LOCK(q);
	while(!q->finish && bound_block_queue_full(q)) {
		condition_wait(&q->no_full);
	}
	assert(q->finish || !bound_block_queue_full(q));
	if(q->finish) {
		ret = false;
	} else {
		QUEUE_INSERT_TAIL(&q->queue_head, node);
		++ q->size;
	}
	condition_notify(&q->no_empty);
	MUTEX_UNLOCK(q);

	return ret;
}

queue_node *bound_block_queue_pop(bound_block_queue *q) {
	assert(q != NULL);
	queue_node *node = NULL;
	MUTEX_LOCK(q);
	while(!q->finish && bound_block_queue_empty(q)) {
		condition_wait(&q->no_empty);
	}
	assert(q->finish || !bound_block_queue_empty(q));
	if(!bound_block_queue_empty(q)) {
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		-- q->size;
	}

	condition_notify(&q->no_full);
	MUTEX_UNLOCK(q);

	return node;
}

void bound_block_queue_finish(bound_block_queue *q) {
	assert(q != NULL);
	MUTEX_LOCK(q);
	q->finish = true;
	condition_notify_all(&q->no_full);
	condition_notify_all(&q->no_empty);
	MUTEX_UNLOCK(q);
}

void bound_block_queue_set_capacity(bound_block_queue *q, int capacity) {
	assert(q != NULL);
	ATOM_SET_OLD(&q->capacity, capacity);
}

void bound_block_queue_clear(bound_block_queue *q, queue_node_free_cb free_cb) {
	assert(q != NULL);
	queue_node *node = NULL;
	if(NULL == q)
		return;

	MUTEX_LOCK(q);
	while(!QUEUE_EMPTY(&q->queue_head)) {
		/* 不能用pop否则会阻塞 */
		// node = bound_block_queue_pop(q);
		node = QUEUE_HEAD(&q->queue_head);
		QUEUE_REMOVE(node);
		// 不能对node直接释放，因为node不是malloc直接分配的地址
		free_cb(node);
	}
	q->size = 0;
	MUTEX_UNLOCK(q);
}

/* 非线程安全 */
void bound_block_queue_destroy(bound_block_queue **q, queue_node_free_cb free_cb) {
	if(q && *q) {
		bound_block_queue_clear(*q, free_cb);
		MUTEX_DESTROY(*q);
		condition_destroy(&(*q)->no_full);
		condition_destroy(&(*q)->no_empty);
		free(*q);
		*q = NULL;
	}
}

