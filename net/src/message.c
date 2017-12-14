/*
 * message.c
 *
 *  Created on: 2017年11月8日
 *      Author: linzer
 */
#include <define.h>
#include <errcode.h>
#include <atomic.h>
#include <queue.h>
#include <tire.h>
#include <spinlock.h>
#include <service.h>
#include <context.h>
#include <message.h>

static uint64_t g_genSessionID = 0;

typedef struct session_cache {
	idtree tree;
	spinlock lock;
} session_cache;

typedef struct session_cb {
	call_success_cb scb;
	call_faild_cb fcb;
} session_cb;

static session_cache *SC = NULL;

int session_cache_init() {
	int errcode = ERROR_FAILD;
	MALLOC_DEF(sc, session_cache);
	if(TEST_VAILD_PTR(sc)) {
		SPIN_INIT(sc);
		idtree_init(&sc->tree);
		SC = sc;
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

void session_cache_release() {
	CHECK_VAILD_PTR(SC);
	SPIN_LOCK(SC);
	idtree_release(&SC->tree);
	SPIN_UNLOCK(SC);
	SPIN_DESTROY(SC);
	FREE(SC);
}

static void session_cache_insert(uint64_t session, session_cb *cb) {
	CHECK_VAILD_PTR(cb);
	CHECK_VAILD_PTR(SC);
	SPIN_LOCK(SC);
	idtree_insert(&SC->tree, session, cb);
	SPIN_UNLOCK(SC);
}

static session_cb *session_cache_remove(uint64_t session) {
	CHECK_VAILD_PTR(SC);
	SPIN_LOCK(SC);
	session_cb *cb = idtree_remove(&SC->tree, session);
	SPIN_UNLOCK(SC);

	return cb;
}

message *quick_gen_msg(HANDLE self, int session, void *data, size_t size, int type) {
	MALLOC_DEF(msg, message);
	STRUCT_ZERO(msg);
	if(TEST_VAILD_PTR(msg)) {
		msg->type = type;
		msg->source = service_get_harborid(self);
		msg->session = session;
		msg->data = data;
		msg->size = size;
	}

	return msg;
}

extern __thread HANDLE t_selfHandle;

int qsend(const char*sname, void *data, size_t size, bool share) {
	int type = MSG_RAW;
	type |= share ? MSG_SHA : MSG_CPY ;
	HANDLE peer = service_get_handle(sname);
	message *msg = quick_gen_msg(t_selfHandle, 0, data, size, type);
	int errcode = ERROR_FAILD;
	if(TEST_VAILD_PTR(msg)) {
		service_push_message(peer, msg);
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

int qcall(const char*sname, void *data, size_t size,
		call_success_cb scb, call_faild_cb fcb) {
	int errcode = ERROR_FAILD;
	int type = MSG_REQ | MSG_CPY;
	HANDLE peer = service_get_handle(sname);
	int session = ATOM_INC_NEW(&g_genSessionID);
	while(!session) {
		session = ATOM_INC_NEW(&g_genSessionID);
	}
	CHECK(session != 0);
	message *msg = quick_gen_msg(t_selfHandle, session, data, size, type);

	if(TEST_VAILD_PTR(msg)) {
		MALLOC_DEF(cb, session_cb);
		if(TEST_VAILD_PTR(cb)) {
			cb->scb = scb;
			cb->fcb = fcb;
			session_cache_insert(session, cb);
			service_push_message(peer, msg);
			errcode = ERROR_SUCCESS;
		}

		FREE(msg);
	}

	return errcode;
}

int qreturn(HANDLE peer, int err,
		void *data, size_t size, int session) {
	int type = MSG_RAW | MSG_CPY;
	int errcode = ERROR_FAILD;
	CHECK(session != 0);
	message *msg = quick_gen_msg(t_selfHandle, session, data, size, type);

	if(TEST_VAILD_PTR(msg)) {
		msg->errcode = err;
		session_cb *cb = session_cache_remove(session);
		CHECK_VAILD_PTR(cb);
		if(TEST_SUCCESS(err)) {
			cb->scb(msg);
		} else {
			cb->fcb(msg);
		}
		FREE(cb);
		service_push_message(peer, msg);
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

