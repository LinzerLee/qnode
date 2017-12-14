/*
 * buffer.h
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#ifndef __QNODE_BUFFER_H__
#define __QNODE_BUFFER_H__
#include <define.h>
#include <stringpiece.h>

FORWARD_DECLAR(buffer)

int buffer_get_prependable(buffer *buf);
int buffer_get_readable(buffer *buf);
int buffer_get_writable(buffer *buf);
buffer *buffer_create(size_t initialSize);
void buffer_destroy(buffer **buf);
void buffer_swap(buffer *buf, buffer *other);
char* buffer_get_beginwrite(buffer *buf);
char *buffer_peek(buffer *buf);
char *buffer_find_crlf(buffer *buf);
char *buffer_find_crlffrom(buffer *buf, const char* start);
char *buffer_find_eol(buffer *buf);
char *buffer_find_eolfrom(buffer *buf, const char* start);
void buffer_retrieve_all(buffer *buf);
void buffer_retrieve(buffer *buf, size_t len);
void buffer_retrieve_until(buffer *buf, const char* end);
void buffer_retrieveInt64(buffer *buf);
void buffer_retrieve_int32(buffer *buf);
void buffer_retrieve_int16(buffer *buf);
void buffer_retrieve_int8(buffer *buf);
stringpiece buffer_retrieve_strpie(buffer *buf, size_t len);
stringpiece buffer_retrieve_all_strpie(buffer *buf);
stringpiece buffer_to_strpie(buffer *buf);
void buffer_make_space(buffer *buf, size_t len);
void buffer_ensure_writable(buffer *buf, size_t len);
void buffer_has_Written(buffer *buf, size_t len);
void buffer_unwrite(buffer *buf, size_t len);
void buffer_append_bytes(buffer *buf, const char *data, size_t len);
void buffer_append_strpie(buffer *buf, stringpiece strpie);
void buffer_append_int64(buffer *buf, int64_t x);
void buffer_append_int32(buffer *buf, int32_t x);
void buffer_append_int16(buffer *buf, int16_t x);
void buffer_append_int8(buffer *buf, int8_t x);
int64_t buffer_read_int64(buffer *buf);
int32_t buffer_read_int32(buffer *buf);
int16_t buffer_read_int16(buffer *buf);
int8_t buffer_read_int8(buffer *buf);
int64_t buffer_peek_int64(buffer *buf);
int32_t buffer_peek_int32(buffer *buf);
int16_t buffer_peek_int16(buffer *buf);
int8_t buffer_peek_int8(buffer *buf);
void buffer_prepend(buffer *buf, const char *data, size_t len);
void buffer_prepend_int64(buffer *buf, int64_t x);
void buffer_prepend_int32(buffer *buf, int32_t x);
void buffer_prepend_int16(buffer *buf, int16_t x);
void buffer_prepend_int8(buffer *buf, int8_t x);
void buffer_shrink(buffer *buf, size_t reserve);
buffer *buffer_steal(buffer *buf);
size_t buffer_get_capacity(buffer *buf);
size_t buffer_read_fromfd(buffer *buf, int fd, int* savedErrno);

#endif /* __QNODE_BUFFER_H__ */
