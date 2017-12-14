/*
 * timestamp.h
 *
 *  Created on: 2017年10月20日
 *      Author: linzer
 */

#ifndef __QNODE_TIMESTAMP_H__
#define __QNODE_TIMESTAMP_H__

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <sys/time.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <define.h>

#define  MICRO_SECOND_PER_SECOND		(1000 * 1000)

typedef struct {
	int64_t us;    /* micro seconds since epoch*/
} timestamp;

STATIC_ASSERT(sizeof(time_t) == sizeof(int64_t), 结构大小检测失败);

static inline timestamp timestamp_invaild() {
	timestamp invaild = { 0 };

	return invaild;
}

static inline timestamp
timestamp_from_unix_time(time_t t) {
	timestamp ts = timestamp_invaild();
	ts.us = (int64_t)t * MICRO_SECOND_PER_SECOND;

	return ts;
}

static inline time_t
timestamp_to_unix_time(timestamp ts) {
	return (time_t) ts.us / MICRO_SECOND_PER_SECOND;
}

static inline timestamp
timestamp_delay(timestamp ts, double s) {
	timestamp ret = timestamp_invaild();
	int64_t delta = (int64_t)(s * MICRO_SECOND_PER_SECOND);
	ret.us = ts.us + delta;

	return ret;
}

static inline double
timestamp_diff(timestamp ts1, timestamp ts2) {
	int64_t diff = ts1.us - ts2.us;
	return ((double) diff) / MICRO_SECOND_PER_SECOND;
}

static inline timestamp timestamp_now() {
	timestamp ts = timestamp_invaild();
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t seconds = tv.tv_sec;

	ts.us = seconds * MICRO_SECOND_PER_SECOND + tv.tv_usec;

	return ts;
}

static inline int
timestamp_compare(timestamp ts1, timestamp ts2) {
	int64_t ret = ts1.us - ts2.us;
	int flag = ret >> ((sizeof(ret) << 3) - 1);

	return ret ? (flag ? -1 : 1) : 0 ;
}

static inline void
timestamp_to_string(timestamp ts, char *buf) {
	int64_t seconds = ts.us / MICRO_SECOND_PER_SECOND;
	int64_t microseconds = ts.us % MICRO_SECOND_PER_SECOND;
	snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
}

static inline void
timestamp_format_string(timestamp ts, char *buf) {
	struct tm tm_time;
	time_t seconds = (time_t) (ts.us / MICRO_SECOND_PER_SECOND);
	int ms = (int) (ts.us % MICRO_SECOND_PER_SECOND);
	gmtime_r(&seconds, &tm_time);
	snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
	         tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
	         tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, ms);
}
#endif /* __QNODE_TIMESTAMP_H__ */
