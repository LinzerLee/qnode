/*
 * net_timer.c
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#include <heap.h>
#include <atomic.h>
#include <define.h>
#include <array.h>
#include <spinlock.h>
#include <net_timer.h>

static atomic_t g_genSequence = { 0 };

typedef struct net_timer{
	timer_entry entry;
	timestamp expiration;
	double interval;
	bool repeat;
	int64_t sequence;
	heap_node node;
} net_timer;

int timer_compare(const heap_node *a, const heap_node *b) {
	const net_timer *timera = DATA(a, net_timer, node);
	const net_timer *timerb = DATA(b, net_timer, node);

	return timestamp_compare(timera->expiration, timerb->expiration);
}

net_timer *timer_create(timer_entry entry, timestamp when, double interval) {
	MALLOC_DEF(timer, net_timer);
	if(TEST_VAILD_PTR(timer)) {
		timer->entry = entry;
		timer->expiration = when;
		timer->interval = interval;
		timer->repeat = interval > 0.0;
		timer->sequence = atomic_inc(&g_genSequence);
		NUL(timer->node.left);
		NUL(timer->node.right);
		NUL(timer->node.parent);
	}

	return timer;
}

void timer_destroy(net_timer **timer) {
	if(TEST_VAILD_PTR(timer) &&
			TEST_VAILD_PTR(*timer)) {
		FREE(*timer);
	}
}

void timer_run(net_timer *timer) {
	CHECK_VAILD_PTR(timer);
	timer->entry.callback(timer->entry.args);
}


void timer_restart(net_timer *timer, timestamp now) {
	CHECK_VAILD_PTR(timer);
	if (timer->repeat) {
		timer->expiration = timestamp_delay(now, timer->interval);
	} else {
		timer->expiration = timestamp_invaild();
	}
}

typedef struct net_timermanager {
	net_eventloop* loop;
	heap timer_heap;
	spinlock lock;
} net_timermanager;

net_timermanager *timermanager_create(net_eventloop* loop) {
	CHECK_VAILD_PTR(loop);
	MALLOC_DEF(manager, net_timermanager);
	if(TEST_VAILD_PTR(manager)) {
		manager->loop = loop;
		heap_init(&manager->timer_heap, timer_compare);
		SPIN_INIT(manager);
	}

	return manager;
}

void timermanager_destroy(net_timermanager **manager) {
	if(TEST_VAILD_PTR(manager) &&
			TEST_VAILD_PTR(manager)) {
		(*manager)->loop = NULL;
		SPIN_DESTROY(*manager);
		heap_node *node = NULL;
		net_timer *timer = NULL;
		while(TEST_VAILD_PTR(node = heap_min(&(*manager)->timer_heap))) {
			heap_dequeue(&(*manager)->timer_heap);
			timer = DATA(node, net_timer, node);
			timer_destroy(&timer);
		}

		FREE(*manager);
	}
}

int timermanager_timeout(net_timermanager *manager) {
	CHECK_VAILD_PTR(manager);
	int timeout = -1;	/* block indefinitely */
	SPIN_LOCK(manager);
	heap_node *node = heap_min(&manager->timer_heap);
	if(TEST_VAILD_PTR(node)) {
		net_timer *timer = DATA(node, net_timer, node);
		timeout = (int) 1000 * timestamp_diff(timer->expiration, timestamp_now());
		timeout = timeout <= 0 ? 0 : timeout;
	}
	SPIN_UNLOCK(manager);

	return timeout;
}

void timermanager_handle_expired(net_timermanager *manager) {
	CHECK_VAILD_PTR(manager);
	ARRAY_CREATE(expired);
	timestamp now = timestamp_now();
	SPIN_LOCK(manager);
	heap_node *node = NULL;
	net_timer *timer = NULL;
	do {
		node = heap_min(&manager->timer_heap);
		if(TEST_VAILD_PTR(node)) {
			timer = DATA(node, net_timer, node);
			if(timestamp_compare(timer->expiration, now) <= 0) {
				heap_dequeue(&manager->timer_heap);
				ARRAY_PUSH_BACK(expired, net_timer *, timer);
			} else {
				break;
			}
		} else {
			break;
		}
	} while(true);
	SPIN_UNLOCK(manager);

	while(ARRAY_SIZE(expired, net_timer *)) {
		timer = ARRAY_POP_BACK_REF(expired, net_timer *);
		timer_run(timer);
		timer_destroy(&timer);
	}

	ARRAY_DESTROY(expired);
}

net_timerid timermanager_add_timer(net_timermanager *manager, timer_entry entry,
		timestamp when, double interval) {
	CHECK_VAILD_PTR(manager);
	CHECK_VAILD_PTR(entry.callback);
	net_timerid id;
	id.timer = NULL;
	id.sequence = 0;
	net_timer *timer = timer_create(entry, when, interval);
	if(TEST_VAILD_PTR(timer)) {
		SPIN_LOCK(manager);
		heap_insert(&manager->timer_heap, &timer->node);
		SPIN_UNLOCK(manager);
		id.timer = timer;
		id.sequence = timer->sequence;
	}

	return id;
}

bool timermanager_cancel_timer(net_timermanager *manager, net_timerid id) {
	IGNORE(manager);
	IGNORE(id);
	// 待实现了map数据结构再来支持取消
	return false;
}
