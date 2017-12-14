/*
 * http.c
 *
 *  Created on: 2017年11月1日
 *      Author: linzer
 */
#include <unistd.h>

#include <define.h>
#include <errcode.h>
#include <context.h>
#include <stringpiece.h>
#include <net.h>
#include <message.h>
#include <service.h>
#include <logger.h>

static inline int http_init(HANDLE service_handle) {
	int errcode = ERROR_SUCCESS;
	service_switch_type(service_handle, TYPE_SERVLET);
	CHECK_SUCCESS(service_register_port(service_handle, 8080, NULL));
	return errcode;
}

static inline int http_release(HANDLE service_handle) {
	return ERROR_SUCCESS;
}

static inline int http_start(HANDLE service_handle) {
	message *msg = service_pop_message(service_handle);
	buffer *buf = (buffer *)msg->data;
	/*
	LOGGER_RECORD_INFO
	LOGGER_RECORD_BYTES(buffer_peek(buf), buffer_get_readable(buf))
	LOGGER_RECORD_FINISH
	*/
	/*
	stringpiece strpie = buffer_to_strpie((buffer *)msg->data);
	printf("message   :\n"
			"\ttype   : %d\n"
			"\tsource : %d\n"
			"\tdata   : %s\n"
			"\tsize   : %zd\n",
			msg->type, msg->source, stringpiece_to_cstring(&strpie), msg->size);
	stringpiece_release(&strpie);
	*/
	if(msg->size == 0 && msg->data) {
		printf("port %d recv %zd bytes data(buffer)\n", msg->sockfd, buffer_get_readable(buf));
		if(MSG_IS_CPY(msg)) {
			buffer_destroy((buffer **)&msg->data);
			FREE(msg);
		}
	} else if(msg->data) {
		printf("port %d recv %zd bytes data(raw)\n", msg->sockfd, msg->size);
		if(MSG_IS_CPY(msg)) {
			FREE(msg->data);
			FREE(msg);
		}
	}

	return ERROR_SUCCESS;
}

static inline int http_stop(HANDLE service_handle) {
	return ERROR_SUCCESS;
}

// TODO
int http_signal(HANDLE service_handle, signalno no) {
	int errcode = ERROR_SUCCESS;
	switch(no) {
	case SIG_SERVICE_INIT:
		errcode = http_init(service_handle);
		break;
	case SIG_SERVICE_START :
		errcode = http_start(service_handle);
		break;
	case SIG_SERVICE_STOP:
		errcode = http_stop(service_handle);
		break;
	case SIG_SERVICE_RELEASE:
		errcode = http_release(service_handle);
		break;
	default:
		break;
	}

	return errcode;
}
