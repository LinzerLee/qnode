/*
 * logger.c
 *
 *  Created on: 2017年10月22日
 *      Author: linzer
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errcode.h>
#include <stringpiece.h>
#include <timestamp.h>
#include <thread.h>
#include <mutex.h>
#include <process.h>
#include <util.h>
#include <message.h>
#include <service.h>
#include <logger.h>

typedef struct logger_buffer{
	void (*cookie)();
	int size;
	int capacity;
	char *data;
} logger_buffer;

static const int MIX_LOG_BUF_SIZE 	= 1000;
static const int SMALL_LOG_BUF_SIZE 	= 4000;
static const int LARGE_LOG_BUF_SIZE 	= 4000 * 1000;

static inline int logger_buffer_init(logger_buffer *buf, int capacity) {
	int errcode = ERROR_FAILD;

	if(capacity < MIX_LOG_BUF_SIZE) {
		capacity = MIX_LOG_BUF_SIZE;
	}

	if(TEST_VAILD_PTR(buf)) {
		MALLOC_SIZE(buf->data, capacity);
		if(TEST_VAILD_PTR(buf->data)) {
			buf->cookie = NULL;
			buf->size = 0;
			buf->capacity = capacity;
			errcode = ERROR_SUCCESS;
		}
	}

	return errcode;
}

static inline int logger_buffer_size(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buf->size;
}

static inline int logger_buffer_capacity(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buf->capacity;
}

static inline char *logger_buffer_data(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buf->data;
}

static inline int logger_buffer_avail(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return logger_buffer_capacity(buf) - logger_buffer_size(buf);
}

static inline char *logger_buffer_current(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return logger_buffer_data(buf) + logger_buffer_size(buf);
}

static inline void
logger_buffer_offset(logger_buffer *buf, int offset) {
	CHECK_VAILD_PTR(buf);
	int newsize = buf->size + offset;
	if(newsize < 0) {
		newsize = 0;
	} else if(newsize > buf->capacity) {
		newsize = buf->capacity;
	}
	buf->size = newsize;
}

static inline void
logger_buffer_steal(logger_buffer *buf, char **sbuff, size_t *len) {
	CHECK_VAILD_PTR(buf);
	char *newbuf;
	MALLOC_SIZE(newbuf, buf->capacity);
	if(TEST_VAILD_PTR(newbuf)) {
		*sbuff = buf->data;
		*len = buf->size;
		buf->data = newbuf;
		buf->size = 0;
	} else {
		NUL(*sbuff);
		*len = 0;
	}
}

static inline void
logger_buffer_append(logger_buffer *buf, const char *data, size_t len) {
	printf("logger get byte %d(%d)\n", (int)len, logger_buffer_avail(buf));
	CHECK_VAILD_PTR(buf);
    if (logger_buffer_avail(buf) > len) {
    		memcpy(logger_buffer_current(buf), data, len);
    		logger_buffer_offset(buf, len);
    } else {
    		char *bbuf = NULL;
    		size_t size;
    		logger_buffer_steal(buf, &bbuf, &size);
    		if(TEST_VAILD_PTR(bbuf)) {
    			qsend("log", bbuf, size, false);
    		}

    		if (logger_buffer_avail(buf) > len) {
    			logger_buffer_append(buf, data, len);
    		} else {
    			fprintf(stderr, "log record is too large, drop this record.\n");
    		}
    }
}

static inline int logger_buffer_resize(logger_buffer *buf, int size) {
	CHECK_VAILD_PTR(buf);
	if(logger_buffer_capacity(buf) >= size) {
		buf->size = size;
	}
	return logger_buffer_size(buf);
}

static inline void logger_buffer_reset(logger_buffer *buf) {
	CHECK_VAILD_PTR(buf);
	logger_buffer_resize(buf, 0);
}

static inline void logger_buffer_release(logger_buffer *buf) {
	if(TEST_VAILD_PTR(buf)) {
		logger_buffer_reset(buf);
		NUL(buf->cookie);
		FREE(buf->data);
	}
}

static inline void logger_buffer_bzero(logger_buffer *buf) {
	assert(buf != NULL);
	bzero(logger_buffer_data(buf), logger_buffer_capacity(buf));
}

// for used by GDB
static inline const char *logger_buffer_debug_string(logger_buffer *buf) {
	assert(buf != NULL);
	char *cur = logger_buffer_current(buf);
	*cur = '\0';
	return logger_buffer_data(buf);
}

static inline void
logger_buffer_set_cookie(logger_buffer *buf, void (*cookie)()) {
	assert(buf != NULL);
	buf->cookie = cookie;
}
// for used by unit test
static inline stringpiece logger_buffer_to_strpie(logger_buffer *buf) {
	assert(buf != NULL);
	stringpiece strpie;
	stringpiece_init_cstring(&strpie, logger_buffer_debug_string(buf));
	return strpie;
}

static const int MAX_NUM_SIZE = 32;

static const char digits[] = "9876543210123456789";
static const char* zero = digits + 9;
STATIC_ASSERT(sizeof(digits) == 20, digits结构大小不对);

const char digitsHex[] = "0123456789ABCDEF";
STATIC_ASSERT(sizeof digitsHex == 17, digitsHex结构大小不对);

static void reverse_str(char s[]) {
	int c, j, i;

	for(i=0, j=strlen(s)-1; i<j; i++, j--) {
		c=s[i];
		s[i]=s[j];
		s[j]=c;
	}
}

static size_t convert_ll(char buf[], long long value) {
  long long i = value;
  char* p = buf;

  do {
    int lsd = (int)(i % 10);
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0)
  {
    *p++ = '-';
  }
  *p = '\0';
  reverse_str(buf);

  return p - buf;
}

static size_t convert_hex(char buf[], uintptr_t value) {
  uintptr_t i = value;
  char* p = buf;

  do {
	int lsd = (int)(i % 16);
    i /= 16;
    *p++ = digitsHex[lsd];
  } while (i != 0);

  *p = '\0';
  reverse_str(buf);

  return p - buf;
}

typedef struct {
	logger_buffer buffer;
} logger_stream;

static inline logger_stream *logger_stream_create() {
	MALLOC_DEF(stream, logger_stream);
	if(TEST_VAILD_PTR(stream)) {
		if(!TEST_SUCCESS(logger_buffer_init(&stream->buffer, SMALL_LOG_BUF_SIZE))) {
			FREE(stream);
		}
	}

	return stream;
}

static inline logger_buffer *logger_stream_buffer(logger_stream *stream) {
	CHECK_VAILD_PTR(stream);
	return &stream->buffer;
}

static inline void
logger_stream_append(logger_stream *stream, const char* data, int len) {
	CHECK_VAILD_PTR(stream);
	logger_buffer_append(logger_stream_buffer(stream), data, len);
}

static inline void logger_stream_reset_buffer(logger_stream *stream) {
	CHECK_VAILD_PTR(stream);
	logger_buffer_reset(logger_stream_buffer(stream));
}

static inline void logger_stream_boolean(logger_stream *stream, int value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer_append(logger_stream_buffer(stream), value ? "1" : "0", 1);
}

static inline void logger_stream_integer(logger_stream *stream, long long value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	if (logger_buffer_avail(buffer) >= MAX_NUM_SIZE) {
		size_t len = convert_ll(logger_buffer_current(buffer), value);
		logger_buffer_offset(buffer, len);
	}
}

static inline void logger_stream_double(logger_stream *stream, double value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	if (logger_buffer_avail(buffer) >= MAX_NUM_SIZE) {
		int len = snprintf(logger_buffer_current(buffer), MAX_NUM_SIZE, "%.12g", value);
		logger_buffer_offset(buffer, len);
	}
}

static inline void logger_stream_char(logger_stream *stream, char value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	logger_buffer_append(buffer, &value, 1);
}

static inline void logger_stream_cstring(logger_stream *stream, const char *value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	if(value) {
		logger_buffer_append(buffer, value, strlen(value));
	} else {
		logger_buffer_append(buffer, "(null)", 6);
	}
}

static inline void logger_stream_bytes(logger_stream *stream, const char *value, int size) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	logger_buffer_append(buffer, value, size);
}

static inline void logger_stream_strpie(logger_stream *stream, const stringpiece *value) {
	CHECK_VAILD_PTR(stream);
	logger_stream_cstring(stream, stringpiece_to_cstring(value));
}

static inline void logger_stream_logbuf(logger_stream *stream, logger_buffer *value) {
	CHECK_VAILD_PTR(stream);
	stringpiece strpie = logger_buffer_to_strpie(value);
	logger_stream_strpie(stream, &strpie);
	stringpiece_release(&strpie);
}

static inline void logger_stream_memaddr(logger_stream *stream, const void *value) {
	CHECK_VAILD_PTR(stream);
	logger_buffer *buffer = logger_stream_buffer(stream);
	uintptr_t v = (uintptr_t) value;
	if (logger_buffer_avail(buffer) >= MAX_NUM_SIZE) {
		char *cur = logger_buffer_current(buffer);
		cur[0] = '0';
		cur[1] = 'x';
		size_t len = convert_hex(cur + 2, v);
		logger_buffer_offset(buffer, len);
	}
}

static inline void logger_stream_destroy(logger_stream **stream) {
	if(stream && *stream) {
		logger_buffer_release(&(*stream)->buffer);
		FREE(*stream);
	}
}

static const int ROLL_PER_SECOND = 60 * 60 * 24;

/* logger storage */
typedef struct {
	stringpiece basename;
	size_t rollSize;
	int flushInterval;
	int checkEveryN;
	int count;
	mutex lock;
	int enableLock;
	time_t startOfPeriod;
	time_t lastRoll;
	time_t lastFlush;
	appendfile *af;
} logger_storage;

static inline stringpiece gen_logfile_name(const stringpiece *basename, time_t* now) {
	assert(basename != NULL);
	assert(now != NULL);
	stringpiece filename;
	stringpiece_init_strpie(&filename, basename);
	char timebuf[32];
	struct tm tm;
	*now = time(NULL);
	gmtime_r(now, &tm); // FIXME: localtime_r ?
	strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
	stringpiece_append(&filename, timebuf);
	stringpiece hostname;
	stringpiece_init(&hostname);
	proc_get_hostname(&hostname);
	stringpiece_append(&filename, stringpiece_to_cstring(&hostname));
	stringpiece_release(&hostname);
	char pidbuf[32];
	snprintf(pidbuf, sizeof pidbuf, ".%d", proc_get_pid());
	stringpiece_append(&filename, pidbuf);
	stringpiece_append(&filename, ".log");

	return filename;
}

static inline bool logger_storage_rollfile(logger_storage *storage) {
	assert(storage != NULL);
	time_t now = time(NULL);
	stringpiece filename = gen_logfile_name(&storage->basename, &now);

	time_t start = now / ROLL_PER_SECOND * ROLL_PER_SECOND;

	if (now > storage->lastRoll) {
		storage->lastRoll = now;
		storage->lastFlush = now;
		storage->startOfPeriod = start;
		if(storage->af) {
			appendfile_close(storage->af);
		}

		storage->af = appendfile_open(stringpiece_to_cstring(&filename));
		if(!TEST_VAILD_PTR(storage->af)) {
			printf("open logfile %s faild\n", stringpiece_to_cstring(&filename));
		} else {
			printf("new logfile %s success\n", stringpiece_to_cstring(&filename));
		}
	    return true;
	}

	return false;
}

static inline logger_storage *
logger_storage_create(stringpiece *basename, size_t rollSize,
					int threadSafe, int flushInterval, int checkEveryN) {
	assert(basename != NULL);
	logger_storage *storage = (logger_storage *)malloc(sizeof(logger_storage));
	if(storage) {
		if(TEST_SUCCESS(stringpiece_init_strpie(&storage->basename, basename))) {
			storage->rollSize = rollSize;
			storage->enableLock = threadSafe;
			storage->flushInterval = flushInterval;
			storage->checkEveryN = checkEveryN;
			storage->count = 0;
			storage->startOfPeriod = 0;
			storage->lastFlush = 0;
			storage->lastRoll = 0;
			MUTEX_INIT(storage);
			int pos = stringpiece_rfind_char(&storage->basename, '/');
			if(STRINGPIECE_NOPOS != pos) {
				stringpiece_remove_prefix(&storage->basename, pos + 1);
			}
			(void)logger_storage_rollfile(storage);
		} else {
			free(storage);
			storage = NULL;
		}
	}

	return storage;
}

static inline void logger_storage_flush(logger_storage *storage) {
	assert(storage != NULL);
	if(storage->enableLock) {
		MUTEX_LOCK(storage);
		appendfile_flush(storage->af);
		MUTEX_UNLOCK(storage);
	} else {
		appendfile_flush(storage->af);
	}
}

static inline void
logger_storage_append(logger_storage *storage, const char* logline,
		int len, bool bLock/* 临时启用或关闭锁 */) {
	assert(storage != NULL);
	assert(logline != NULL);

	if(bLock) {
		MUTEX_LOCK(storage);
	}

	appendfile_append(storage->af, logline, len);
	if(storage->af->writeBytes > storage->rollSize) {
		logger_storage_rollfile(storage);
	} else {
		++ storage->count;
		if(storage->count >= storage->checkEveryN) {
			storage->count = 0;
			time_t now = time(NULL);
			time_t thisPeriod_ = now / ROLL_PER_SECOND * ROLL_PER_SECOND;
			if (thisPeriod_ != storage->startOfPeriod) {
				logger_storage_rollfile(storage);
			} else if (now - storage->lastFlush > storage->flushInterval) {
				storage->lastFlush = now;
				appendfile_flush(storage->af);
			}
		}
	}
	if(bLock) {
		MUTEX_UNLOCK(storage);
	}
}

static inline void
logger_storage_destroy(logger_storage **storage) {
	if(storage && *storage) {
		stringpiece_release(&(*storage)->basename);
		appendfile_close((*storage)->af);
		FREE(*storage);
	}
}

__thread static char t_time[32];
__thread static time_t t_lastSecond;

static const int DEFAULT_ROLL_SIZE = 1024 * 1024 * 300;
static const int DEFAULT_ENABLE_LOCK = true;
static const int DEFAULT_FLUSH_INTERVAL = 3;
static const int DEFAULT_CHECK_EVERY_N = 1024;

static const char *g_logLevelName[MAX_LOG_LEVELS] = {
	"TRACE ",
	"DEBUG ",
	"INFO  ",
	"WARN  ",
	"ERROR ",
	"FATAL ",
};

typedef struct logger {
	logger_stream *stream;
	LOGGER_LEVEL level;
	int line;
} logger;

static timestamp t_timestamp;
static logger_storage *LS = NULL;
/* console config global logger level */
static LOGGER_LEVEL g_loggerLevel = INFO;
__thread extern HANDLE t_selfHandle;

#define TEST_LOGGER_LEVEL(level)			\
	(level >= g_loggerLevel)

#define TEST_LOGGER_SERVER_INIT()	\
	(LS != NULL)

logger *logger_create() {
	MALLOC_DEF(log, logger);
	if(TEST_VAILD_PTR(log)) {
		log->line = 0;
		log->level = INFO;
		log->stream = logger_stream_create();
		if(TEST_VAILD_PTR(log->stream)) {
			return log;
		}

		FREE(log);
	}

	return log;
}

void logger_destroy(logger **log) {
	if(TEST_VAILD_PTR(log) && TEST_VAILD_PTR(*log)) {
		logger_stream_destroy(&(*log)->stream);
		FREE(*log);
	}
}

void logger_server_init() {
	stringpiece basename;
	stringpiece_init(&basename);
	proc_get_procname(&basename);
	LS = logger_storage_create(&basename, DEFAULT_ROLL_SIZE,
				DEFAULT_ENABLE_LOCK, DEFAULT_FLUSH_INTERVAL, DEFAULT_CHECK_EVERY_N);
	stringpiece_release(&basename);
}

void logger_server_release() {
	logger_storage_destroy(&LS);
}

static inline logger *inner_get_logger() {
	return service_get_logger(t_selfHandle);
}

static inline void logger_update() {
	(void)thread_current_id();
	t_timestamp = timestamp_now();
}
/*输出格式化时间*/
static inline void logger_format_datetime() {
	logger *log = inner_get_logger();
	CHECK_VAILD_PTR(log);
	int64_t us = t_timestamp.us;
	time_t seconds = (time_t)(us / MICRO_SECOND_PER_SECOND);
	int microseconds = (int)(us % MICRO_SECOND_PER_SECOND);
	if (seconds != t_lastSecond) {
		t_lastSecond = seconds;

		struct tm tm_time;
		gmtime_r(&seconds, &tm_time);
		int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
    				tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		assert(len == 17);
		(void)len;
	}
	char buf[32];
	int length = snprintf(buf, sizeof buf, ".%06dZ ", microseconds);
	assert(length == 9);
	logger_stream_bytes(log->stream, t_time, 17);
	logger_stream_bytes(log->stream, buf, 9);
}

void logger_record_start(LOGGER_LEVEL level) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	log->level = level;
	logger_update();
	logger_format_datetime();
	logger_stream_cstring(log->stream, thread_current_idstring());
	logger_stream_cstring(log->stream, g_logLevelName[level]);
	logger_stream_cstring(log->stream, thread_current_name());
	logger_stream_char(log->stream, ' ');
}

void logger_record_finish(const char *file, const char *func, int line) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}

	logger_stream_cstring(log->stream, " - ");
	if(file) {
		const char* slash = strrchr(file, '/');
		if (slash) {
			slash = slash + 1;
		} else {
			slash = file;
		}
		logger_stream_cstring(log->stream, slash);
		logger_stream_char(log->stream, ':');
	}

	if(func) {
		logger_stream_cstring(log->stream, func);
		logger_stream_char(log->stream, ':');
	}

	if(line) {
		logger_stream_integer(log->stream, '\n');
	}

	logger_stream_char(log->stream, '\n');
}

void logger_boolean(int value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_boolean(log->stream, value);
}

void logger_integer(long long value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_integer(log->stream, value);
}

void logger_double(double value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_double(log->stream, value);
}

void logger_char(char value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_char(log->stream, value);
}

void logger_cstring(const char *value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_cstring(log->stream, value);
}

void logger_bytes(const char *value, int size) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}

	logger_stream_bytes(log->stream, value, size);
}

void logger_strpie(const stringpiece *value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_strpie(log->stream, value);
}

void logger_logbuf(logger_buffer *value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_logbuf(log->stream, value);
}

void logger_memaddr(const void *value) {
	logger *log = inner_get_logger();
	if(!TEST_VAILD_PTR(log) || !TEST_LOGGER_SERVER_INIT()) {
		return;
	}
	logger_stream_memaddr(log->stream, value);
}


void logger_set_level(LOGGER_LEVEL level) {
	g_loggerLevel = level;
}

void logger_server_rollsize(int rollsize) {
	if(TEST_VAILD_PTR(LS)) {
		LS->rollSize = rollsize;
	}
}

void logger_server_flushintvl(int intvl) {
	if(TEST_VAILD_PTR(LS)) {
		LS->flushInterval = intvl;
	}
}

void logger_server_checkfreq(int freq) {
	if(TEST_VAILD_PTR(LS)) {
		LS->checkEveryN = freq;
	}
}

void logger_server_flush() {
	if(TEST_VAILD_PTR(LS)) {
		logger_storage_flush(LS);
	}
}

void logger_server_stor(const char *logline, int len, bool block) {
	if(TEST_VAILD_PTR(LS)) {
		logger_storage_append(LS, logline, len, block);
	}
}
