/*
 * console.c
 *
 *  Created on: 2017年11月10日
 *      Author: linzer
 */

#include <unistd.h>
#include <stdio.h>

#include <define.h>
#include <errcode.h>
#include <context.h>
#include <stringpiece.h>
#include <net.h>
#include <message.h>
#include <service.h>

static inline int console_init(HANDLE service_handle) {
	int errcode = ERROR_SUCCESS;
	// service_register_type(service_handle, TYPE_SERVLET);
	CHECK_SUCCESS(service_register_port(service_handle, 8000, "cmd"));
	return errcode;
}

static inline int console_release(HANDLE service_handle) {

	return ERROR_SUCCESS;
}

static inline int console_start(HANDLE service_handle) {
	message *msg = service_pop_message(service_handle);
	CHECK_VAILD_PTR(msg);
	printf("recv cmd : %s\n", (const char *)msg->data);
	if(MSG_IS_CPY(msg)) {
		FREE(msg->data);
		FREE(msg);
	}

	return ERROR_SUCCESS;
}

static inline int console_stop(HANDLE service_handle) {
	return ERROR_SUCCESS;
}

// TODO
int console_signal(HANDLE service_handle, signalno no) {
	int errcode = ERROR_SUCCESS;
	switch(no) {
	case SIG_SERVICE_INIT:
		errcode = console_init(service_handle);
		break;
	case SIG_SERVICE_START :
		errcode = console_start(service_handle);
		break;
	case SIG_SERVICE_STOP:
		errcode = console_stop(service_handle);
		break;
	case SIG_SERVICE_RELEASE:
		errcode = console_release(service_handle);
		break;
	default:
		break;
	}

	return errcode;
}

