/*
 * logger.h
 *
 *  Created on: 2017年10月22日
 *      Author: linzer
 */

#ifndef __QNODE_LOGGER_H__
#define __QNODE_LOGGER_H__

#include <define.h>
#include <stringpiece.h>

typedef enum {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    MAX_LOG_LEVELS,
} LOGGER_LEVEL;

FORWARD_DECLAR(logger_buffer)
FORWARD_DECLAR(logger)

/* Client API */
#define LOGGER_RECORD_TRACE		logger_record_start(TRACE);
#define LOGGER_RECORD_DEBUG		logger_record_start(DEBUG);
#define LOGGER_RECORD_INFO		logger_record_start(INFO);
#define LOGGER_RECORD_WARN		logger_record_start(WARN);
#define LOGGER_RECORD_ERROR		logger_record_start(ERROR);
#define LOGGER_RECORD_FATAL		logger_record_start(FATAL);
#define LOGGER_RECORD_FINISH		logger_record_finish(__FILE__, __func__, __LINE__);
#define LOGGER_LEVEL_TRACE		logger_set_level(TRACE);
#define LOGGER_LEVEL_DEBUG		logger_set_level(DEBUG);
#define LOGGER_LEVEL_INFO		logger_set_level(INFO);
#define LOGGER_LEVEL_WARN		logger_set_level(WARN);
#define LOGGER_LEVEL_ERROR		logger_set_level(ERROR);
#define LOGGER_LEVEL_FATAL		logger_set_level(FATAL);
#define LOGGER_RECORD(type, value)	logger_##type(value);
#define LOGGER_RECORD_BYTES(value, size)		logger_bytes(value, size);

/* Server API */
#define LOGGER_SERVER_ROLLSIZE(rollsize) 	logger_server_rollsize(rollsize);
#define LOGGER_SERVER_FLUSH_INTVL(intvl) 	logger_server_flushintvl(intvl);
#define LOGGER_SERVER_CHECK_FREQ(freq) 		logger_server_checkfreq(freq);

#define LOGGER_SERVER_LOCK_STOR(ptr, size)		logger_server_stor((ptr), (size), true);
#define LOGGER_SERVER_UNLOCK_STOR(ptr, size)		logger_server_stor((ptr), (size), false);
#define LOGGER_SERVER_INIT						logger_server_init();
#define LOGGER_SERVER_RELEASE					logger_server_release();
#define LOGGER_SERVER_FLUSH						logger_server_flush();
logger *logger_create();
void logger_destroy(logger **log);
void logger_set_level(LOGGER_LEVEL level);
void logger_server_rollsize(int rollsize);
void logger_server_flushintvl(int intvl);
void logger_server_checkfreq(int freq);
void logger_server_flush();
void logger_server_stor(const char *logline, int len, bool block);
void logger_server_init();
void logger_server_release();
void logger_record_start(LOGGER_LEVEL level);
void logger_record_finish(const char *file, const char *func, int line);
void logger_boolean(int value);
void logger_integer(long long value);
void logger_double(double value);
void logger_char(char value);
void logger_cstring(const char *value);
void logger_bytes(const char *value, int size);
void logger_strpie(const stringpiece *value);
void logger_memaddr(const void *value);

/* Server API */
#endif /* __QNODE_LOGGER_H__ */
