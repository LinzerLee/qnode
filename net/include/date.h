/*
 * date.h
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */

#ifndef __QNODE_DATE_H__
#define __QNODE_DATE_H__
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include <define.h>

#define DAYS_PER_WEEK				7

typedef struct {
	int year; 			// [1900..2500]
	int month;  			// [1..12]
	int day;  			// [1..31]
} ymd_date;

typedef struct {
	int julian_num;
} date;

STATIC_ASSERT(sizeof(date) >= sizeof(int32_t), date需要32位整型);

static inline date date_invaild() {
	date invaild = { 0 };

	return invaild;
}

static inline date date_from_ymd(ymd_date ymd) {
	int a = (14 - ymd.month) / 12;
	int y = ymd.year + 4800 - a;
	int m = ymd.month + 12 * a - 3;
	date dt = date_invaild();
	dt.julian_num = ymd.day + (153*m + 2) / 5 + y*365 + y/4 - y/100 + y/400 - 32045;
	return dt;
}

#define JULIAN_DAY_OF_1970_01_01		data_from_ymd(1970, 1, 1)

static inline date date_from_tm(struct tm *tm) {
	ymd_date ymd = { tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday };

	return date_from_ymd(ymd);
}

static inline ymd_date date_to_ymd(date dt) {
	int a = dt.julian_num + 32044;
	int b = (4 * a + 3) / 146097;
	int c = a - ((b * 146097) / 4);
	int d = (4 * c + 3) / 1461;
	int e = c - ((1461 * d) / 4);
	int m = (5 * e + 2) / 153;
	ymd_date ymd;

	ymd.day = e - ((153 * m + 2) / 5) + 1;
	ymd.month = m + 3 - 12 * (m / 10);
	ymd.year = b * 100 + d - 4800 + (m / 10);

	return ymd;
}

static inline int date_get_week(date dt) {
	return (dt.julian_num + 1) % DAYS_PER_WEEK;
}

static inline int date_compare(date dt1, date dt2) {
	return dt1.julian_num - dt2.julian_num;
}

#define ISO_DATE_BUF_SIZE	32

static inline void date_to_iso_string(date dt, char *buf, size_t size) {
	 ymd_date ymd = date_to_ymd(dt);
	 snprintf(buf, size, "%4d-%02d-%02d", ymd.year, ymd.month, ymd.day);
}

static inline date date_today() {
	time_t timer = time(NULL);
	struct tm tm = { 0 };

	localtime_r(&timer, &tm);
	return date_from_tm(&tm);
}
#endif /* __QNODE_DATE_H__ */
