/*
 * context.c
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#include <stdint.h>

#include <errcode.h>
#include <spinlock.h>
#include <queue.h>
#include <stringpiece.h>
#include <net.h>
#include <message.h>
#include <service.h>
#include <context.h>

extern int session_cache_init();
extern int session_cache_release();

typedef struct mailbox {
	block_queue *msg_queue;				/* message queue */
	uint64_t recv;
	uint64_t consume;
} mailbox;

static void inner_destroy_message(queue_node *node) {
	message *msg = DATA(node, message, node);
	if(TEST_VAILD_PTR(msg)) {
		FREE(msg->data);
		FREE(msg);
	}
}

mailbox *mailbox_create() {
	MALLOC_DEF(box, mailbox);
	if(TEST_VAILD_PTR(box)) {
		box->msg_queue = block_queue_create();
		if(TEST_VAILD_PTR(box->msg_queue)) {
			box->consume = 0;
			box->recv = 0;

			return box;
			block_queue_destroy(&box->msg_queue, inner_destroy_message);
		}
	}

	return box;
}

void mailbox_destroy(mailbox **box) {
	if(TEST_VAILD_PTR(box) && TEST_VAILD_PTR(*box)) {
		(*box)->consume = 0;
		(*box)->recv = 0;
		if(TEST_VAILD_PTR((*box)->msg_queue)) {
			block_queue_destroy(&(*box)->msg_queue, inner_destroy_message);
		}
	}
}

void mailbox_send(mailbox *box, message *msg) {
	CHECK_VAILD_PTR(box);
	CHECK_VAILD_PTR(msg);
	block_queue_push(box->msg_queue, &msg->node);
}

message *mailbox_recv(mailbox *box) {
	CHECK_VAILD_PTR(box);
	message *msg = NULL;
	queue_node *node = NULL;
	node = block_queue_pop(box->msg_queue);
	if(TEST_VAILD_PTR(node)) {
		msg = DATA(node, message, node);
	}

	return msg;
}

message *mailbox_try_recv(mailbox *box) {
	CHECK_VAILD_PTR(box);
	message *msg = NULL;
	queue_node *node = NULL;
	node = block_queue_try_pop(box->msg_queue);
	if(TEST_VAILD_PTR(node)) {
		msg = DATA(node, message, node);
	}

	return msg;
}

bool mailbox_has_mail(mailbox *box) {
	CHECK_VAILD_PTR(box);
	return !block_queue_empty(box->msg_queue);
}

typedef struct context {
	net_eventloop *loop;
	spinlock lock;
	uint16_t id;			/* node id */
	mailbox *slots[SERVICE_POOL_SIZE];
} context;

static context *C = NULL;

context *global_context() {
	return C;
}

int context_init() {
	int errcode = session_cache_init();
	if(!TEST_SUCCESS(errcode))
		return errcode;

	MALLOC_DEF(c, context);
	if(TEST_VAILD_PTR(c)) {
		NUL(c->loop);
		SPIN_INIT(c);
		ZERO(c->slots);
		C = c;
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

int context_get_nodeid() {
	CHECK_VAILD_PTR(C);
	return C->id;
}

int context_register_eventloop(net_eventloop *loop) {
	CHECK_VAILD_PTR(C);
	CHECK_VAILD_PTR(loop);
	SPIN_LOCK(C);
	C->loop = loop;
	SPIN_UNLOCK(C);

	return ERROR_SUCCESS;
}

int context_register_mailbox(HANDLE handle) {
	CHECK_VAILD_PTR(C);
	int errcode = ERROR_FAILD;
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	SPIN_LOCK(C);
	if(!TEST_VAILD_PTR(C->slots[handle])) {
		C->slots[handle] = mailbox_create();
	}

	if(TEST_VAILD_PTR(C->slots[handle])) {
		errcode = ERROR_SUCCESS;
	}
	SPIN_UNLOCK(C);

	return errcode;
}

void context_unregister_mailbox(HANDLE handle) {
	CHECK_VAILD_PTR(C);
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	SPIN_LOCK(C);
	if(TEST_VAILD_PTR(C->slots[handle])) {
		mailbox_destroy(&C->slots[handle]);
	}
	SPIN_UNLOCK(C);
}

void context_send_mail(HANDLE handle, message *msg) {
	CHECK_VAILD_PTR(C);
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	mailbox_send(C->slots[handle], msg);
}

message *context_recv_mail(HANDLE handle) {
	CHECK_VAILD_PTR(C);
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	return mailbox_recv(C->slots[handle]);
}

message *context_try_recv_mail(HANDLE handle) {
	CHECK_VAILD_PTR(C);
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	return mailbox_try_recv(C->slots[handle]);
}

bool context_has_mail(HANDLE handle) {
	CHECK_VAILD_PTR(C);
	CHECK(handle >=0 && handle <= INVAILD_SERVICE_HANDLE);
	return mailbox_has_mail(C->slots[handle]);
}

net_eventloop *context_select_eventloop() {
	CHECK_VAILD_PTR(C);

	return C->loop;
}

void context_release() {
	session_cache_release();
	if(TEST_VAILD_PTR(C)) {
		SPIN_LOCK(C);
		NUL(C->loop);

		for(int i=0; i<SERVICE_POOL_SIZE; ++i) {
			if(TEST_VAILD_PTR(C->slots[i])) {
				context_unregister_mailbox(i);
			}
		}
		SPIN_UNLOCK(C);

		FREE(C);
	}
}
