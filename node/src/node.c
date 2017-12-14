/*
 * node.c
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */
#include <stdio.h>
#include <unistd.h>

#include <define.h>
#include <thread.h>
#include <errcode.h>
#include <env.h>
#include <context.h>
#include <service.h>
#include <monitor.h>
#include <module.h>
#include <logger.h>

const char *service_batch[][SERVICE_STAGE_MAX] = {
	{ "log", "gate", NULL },
	{ "console", NULL },
	{ "http", "pinpon", NULL },
	{ NULL }
};

int main() {
	int errcode = ERROR_SUCCESS;
	errcode |= env_init("/Users/linzer/Documents/config");
	errcode |= context_init();
	errcode |= module_init();
	errcode |= service_init();
	errcode |= threadpool_init(NULL);
	errcode |= monitor_init();

	errcode |= service_batch_boost(service_batch);
	/*
	service_boost("gate");
	if(TEST_SUCCESS(service_wait("gate", STAGE_AFTER, SERVICE_INITING, 5))) {
		service_boost("http");
	} else {
		exit(-2);
	}
	*/

	CHECK_SUCCESS(errcode);
	while(true) {
		monitor_update();
		service_dispatch_message();
		service_handle_trash();
	}

	threadpool_stop();
	threadpool_release();

	return ERROR_SUCCESS;
}
