/*
 * gate.c
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#include <define.h>
#include <errcode.h>
#include <context.h>
#include <net.h>
#include <service.h>

static net_eventloop* loop = NULL;

static inline int gate_init(HANDLE service_handle) {
	int errcode = ERROR_SUCCESS;
	service_switch_type(service_handle, TYPE_SERVICE);

	loop = eventloop_create();
	if(!TEST_VAILD_PTR(loop)) {
		errcode = ERROR_FAILD;
		ABORT
	} else {
		context_register_eventloop(loop);
	}


	return errcode;
}

static inline int gate_release(HANDLE service_handle) {
	if(TEST_VAILD_PTR(loop)) {
		eventloop_destroy(&loop);
	}

	return ERROR_SUCCESS;
}

static inline int gate_start(HANDLE service_handle) {
	eventloop_asgin_owner(loop);
	eventloop_run_loop(loop);
	return ERROR_SUCCESS;
}

static inline int gate_stop(HANDLE service_handle) {
	eventloop_quit(loop);
	return ERROR_SUCCESS;
}

// TODO
int gate_signal(HANDLE service_handle, signalno no) {
	int errcode = ERROR_SUCCESS;
	switch(no) {
	case SIG_SERVICE_INIT:
		errcode = gate_init(service_handle);
		break;
	case SIG_SERVICE_START :
		errcode = gate_start(service_handle);
		break;
	case SIG_SERVICE_STOP:
		errcode = gate_stop(service_handle);
		break;
	case SIG_SERVICE_RELEASE:
		errcode = gate_release(service_handle);
		break;
	default:
		break;
	}

	return errcode;
}



