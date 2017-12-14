/*
 * log.c
 *
 *  Created on: 2017年11月11日
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
#include <logger.h>

static inline int log_init(HANDLE service_handle) {
	int errcode = ERROR_SUCCESS;
	LOGGER_SERVER_INIT
	return errcode;
}

static inline int log_release(HANDLE service_handle) {
	int errcode = ERROR_SUCCESS;
	LOGGER_SERVER_RELEASE
	return errcode;
}

static inline int log_start(HANDLE service_handle) {
	message *msg = service_pop_message(service_handle);
	if(TEST_VAILD_PTR(msg)) {
		char *record = msg->data;
		LOGGER_SERVER_UNLOCK_STOR(record, msg->size)
		LOGGER_SERVER_FLUSH
		if(MSG_IS_CPY(msg)) {
			FREE(msg->data);
			FREE(msg);
		}
	}



	return ERROR_SUCCESS;
}

static inline int log_stop(HANDLE service_handle) {
	return ERROR_SUCCESS;
}

// TODO
int log_signal(HANDLE service_handle, signalno no) {
	int errcode = ERROR_SUCCESS;
	switch(no) {
	case SIG_SERVICE_INIT:
		errcode = log_init(service_handle);
		break;
	case SIG_SERVICE_START :
		errcode = log_start(service_handle);
		break;
	case SIG_SERVICE_STOP:
		errcode = log_stop(service_handle);
		break;
	case SIG_SERVICE_RELEASE:
		errcode = log_release(service_handle);
		break;
	default:
		break;
	}

	return errcode;
}


