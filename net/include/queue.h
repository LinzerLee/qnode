/*
 * queue
 *
 *  Created on: 2017年10月18日
 *      Author: linzer
 */

#ifndef __QNODE_QUEUE_H__
#define __QNODE_QUEUE_H__

#include <stdlib.h>
#include <assert.h>

#include <list.h>
#include <spinlock.h>
#include <condition.h>

typedef dclist_node queue_node;
typedef void(* queue_node_free_cb)(queue_node *);

typedef struct {
	queue_node queue_head;
	int size;
	spinlock lock;
} queue;

queue *queue_create();
int queue_empty(const queue *q);
int queue_size(const queue *q);
void queue_push(queue *q, queue_node *node);
queue_node *queue_unpush(queue *q);
queue_node *queue_pop(queue *q);
queue_node *queue_front(queue *q);
queue_node *queue_back(queue *q);
void queue_clear(queue *q, queue_node_free_cb free_cb);
/* 非线程安全 */
void queue_destroy(queue **q, queue_node_free_cb free_cb);

typedef struct {
	atomic_ptr node;
} quick_queue;

quick_queue *quick_queue_create();
int quick_queue_empty(const quick_queue *q);
void quick_queue_push(quick_queue *q, queue_node *node);
queue_node *quick_queue_unpush(quick_queue *q);
queue_node *quick_queue_pop(quick_queue *q);
void quick_queue_clear(quick_queue *q);
/* 非线程安全 */
void quick_queue_destroy(quick_queue **q);

typedef struct {
	queue_node queue_head;
	int size;
	mutex lock;
	condition cond;
	bool finish;
} block_queue;

block_queue *block_queue_create();
int block_queue_empty(const block_queue *q);
int block_queue_size(const block_queue *q);
bool block_queue_push(block_queue *q, queue_node *node);
queue_node *block_queue_pop(block_queue *q);
queue_node *block_queue_try_pop(block_queue *q);
void block_queue_finish(block_queue *q);
void block_queue_clear(block_queue *q, queue_node_free_cb free_cb);
/* 非线程安全 */
void block_queue_destroy(block_queue **q, queue_node_free_cb free_cb);

typedef struct {
	queue_node queue_head;
	mutex lock;
	condition no_empty;
	condition no_full;
	int size;
	int capacity;
	bool finish;
} bound_block_queue;

bound_block_queue *bound_block_queue_create(int capacity);
int bound_block_queue_empty(const bound_block_queue *q);
int bound_block_queue_full(const bound_block_queue *q);
int bound_block_queue_capacity(const bound_block_queue *q);
int bound_block_queue_size(const bound_block_queue *q);
bool bound_block_queue_push(bound_block_queue *q, queue_node *node);
queue_node *bound_block_queue_pop(bound_block_queue *q);
void bound_block_queue_finish(bound_block_queue *q);
void bound_block_queue_set_capacity(bound_block_queue *q, int capacity);
void bound_block_queue_clear(bound_block_queue *q, queue_node_free_cb free_cb);
/* 非线程安全 */
void bound_block_queue_destroy(bound_block_queue **q, queue_node_free_cb free_cb);

#endif /* __QNODE_QUEUE_H__ */
