/*
 * monitor.c
 *
 *  Created on: 2017年11月10日
 *      Author: linzer
 */
#include <errcode.h>
#include <monitor.h>

typedef struct monitor {
	int worker_total;
	int service_total;
	int memory_total;
} monitor;

static monitor *MI = NULL;

int monitor_init() {
	int errcode = ERROR_FAILD;
	MALLOC_DEF(mi, monitor);
	if(TEST_VAILD_PTR(mi)) {
		MI = mi;
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

void monitor_release() {
	if(TEST_VAILD_PTR(MI)) {
		FREE(MI);
	}
}

void monitor_update() {
}
