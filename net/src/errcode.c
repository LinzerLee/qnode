/*
 * errcode.c
 *
 *  Created on: 2017年10月22日
 *      Author: linzer
 */
#include <string.h>
#include <errcode.h>

__thread static char t_errnobuf[ERROR_MSG_BUF_SIZE];

const char* strerror_tl(int errno) {
	strerror_r(errno, t_errnobuf, sizeof(t_errnobuf));
	return t_errnobuf;
}


