/*
 * buffer.c
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */
#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#include <net.h>
#include <define.h>
#include <array.h>
#include <stringpiece.h>
#include <buffer.h>

static const char CRLF[] = "\r\n";

static const size_t CHEAP_PREPEND = 8;
static const size_t INIT_SIZE = 1024;

typedef struct buffer {
	ARRAY buf;
	size_t readerIndex;
	size_t writerIndex;
} buffer;

int buffer_get_prependable(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buf->readerIndex;
}

int buffer_get_readable(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buf->writerIndex - buf->readerIndex;
}

int buffer_get_writable(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return byte_array_capacity(buf->buf) - buf->writerIndex;
}

buffer *buffer_create(size_t initialSize) {
	if(initialSize == 0)
		initialSize = INIT_SIZE;

	initialSize += CHEAP_PREPEND;

	MALLOC_DEF(buf, buffer);
	if(TEST_VAILD_PTR(buf)) {
		buf->readerIndex = CHEAP_PREPEND;
		buf->writerIndex = CHEAP_PREPEND;
		buf->buf = byte_array_create_capacity(initialSize);
		if(TEST_VAILD_PTR(buf->buf)) {
			byte_array_resize(buf->buf, initialSize);
			CHECK(buffer_get_prependable(buf) == CHEAP_PREPEND);
			CHECK(buffer_get_readable(buf) == 0);

			return buf;
		}

		FREE(buf);
	}

	return buf;
}

void buffer_destroy(buffer **buf) {
	if(TEST_VAILD_PTR(buf) && TEST_VAILD_PTR(*buf)) {
		byte_array_destroy(&(*buf)->buf);
		FREE(*buf);
	}
}

void buffer_swap(buffer *buf, buffer *other) {
	CHECK_VAILD_PTR(buf);
	CHECK_VAILD_PTR(other);
	buffer tmp = *buf;
	*buf = *other;
	*other = tmp;
}

static inline char *inner_buffer_begin(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return byte_array_get_bytes(buf->buf);
}

char* buffer_get_beginwrite(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return inner_buffer_begin(buf) + buf->writerIndex;
}

char *buffer_peek(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return inner_buffer_begin(buf) + buf->readerIndex;
}

char *buffer_find_crlf(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
	char *crlf = memmem(buffer_peek(buf), buffer_get_readable(buf), CRLF, 2);

    return crlf;
}

char *buffer_find_crlffrom(buffer *buf, const char* start) {
	CHECK_VAILD_PTR(buf);
	CHECK(buffer_peek(buf) <= start);
	CHECK(start <= buffer_get_beginwrite(buf));

	size_t offset = start - buffer_peek(buf);
	char *crlf = memmem(start, buffer_get_readable(buf) - offset, CRLF, 2);

	return crlf;
}

char *buffer_find_eol(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	char* eol = memchr(buffer_peek(buf), '\n', buffer_get_readable(buf));

	return eol;
}

char *buffer_find_eolfrom(buffer *buf, const char* start) {
	CHECK_VAILD_PTR(buf);
	CHECK(buffer_peek(buf) <= start);
	CHECK(start <= buffer_get_beginwrite(buf));

	char* eol = memchr(start, '\n', buffer_get_beginwrite(buf) - start);

	return eol;
 }


// retrieve returns void, to prevent
// string str(retrieve(readableBytes()), readableBytes());
// the evaluation of two functions are unspecified
void buffer_retrieve_all(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	buf->readerIndex = CHEAP_PREPEND;
	buf->writerIndex = CHEAP_PREPEND;
}

void buffer_retrieve(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK(len <= buffer_get_readable(buf));
	if (len < buffer_get_readable(buf)) {
		buf->readerIndex += len;
	} else {
		buffer_retrieve_all(buf);
	}
}

void buffer_retrieve_until(buffer *buf, const char* end) {
	CHECK_VAILD_PTR(buf);
	CHECK(buffer_peek(buf) <= end);
	CHECK(end <= buffer_get_beginwrite(buf));
	buffer_retrieve(buf, end - buffer_peek(buf));
}

void buffer_retrieve_int64(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	buffer_retrieve(buf, sizeof(int64_t));
}

void buffer_retrieve_int32(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	buffer_retrieve(buf, sizeof(int32_t));
}

void buffer_retrieve_int16(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	buffer_retrieve(buf, sizeof(int16_t));
}

void buffer_retrieve_int8(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	buffer_retrieve(buf, sizeof(int8_t));
}

stringpiece buffer_retrieve_strpie(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK(len <= buffer_get_readable(buf));
	stringpiece result;
	stringpiece_init_buffer(&result, buffer_peek(buf), len);
	buffer_retrieve(buf, len);

    return result;
}

stringpiece buffer_retrieve_all_strpie(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	return buffer_retrieve_strpie(buf, buffer_get_readable(buf));
}

stringpiece buffer_to_strpie(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	stringpiece strpie;
	stringpiece_init_buffer(&strpie, buffer_peek(buf), buffer_get_readable(buf));

	return strpie;
}

void buffer_make_space(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);
	if (buffer_get_writable(buf) + buffer_get_prependable(buf) < len + CHEAP_PREPEND) {
		byte_array_resize(buf->buf, buf->writerIndex + len);
	} else {
		// move readable data to the front, make space inside buffer
		CHECK(CHEAP_PREPEND < buf->readerIndex);
		size_t readable = buffer_get_readable(buf);
		char *begin = inner_buffer_begin(buf);
		memcpy(begin + CHEAP_PREPEND, begin + buf->readerIndex, readable);
		buf->readerIndex = CHEAP_PREPEND;
		buf->writerIndex = buf->readerIndex + readable;
		CHECK(readable == buffer_get_readable(buf));
	}
}

void buffer_ensure_writable(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);

	if (buffer_get_writable(buf) < len) {
		buffer_make_space(buf, len);
	}

	CHECK(buffer_get_writable(buf) >= len);
}

void buffer_has_Written(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK(len <= buffer_get_writable(buf));
	buf->writerIndex += len;
}

void buffer_unwrite(buffer *buf, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK(len <= buffer_get_readable(buf));
	buf->writerIndex -= len;
}

void buffer_append_bytes(buffer *buf, const char *data, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK_VAILD_PTR(data);

	buffer_ensure_writable(buf, len);
	memcpy(buffer_get_beginwrite(buf), data, len);
	buffer_has_Written(buf, len);
}

void buffer_append_strpie(buffer *buf, stringpiece strpie) {
	CHECK_VAILD_PTR(buf);
	buffer_append_bytes(buf, stringpiece_data(&strpie), stringpiece_size(&strpie));
}


///
/// Append int64_t using network endian
///
void buffer_append_int64(buffer *buf, int64_t x) {
	CHECK_VAILD_PTR(buf);
	int64_t be64 = hton64(x);
	buffer_append_bytes(buf, (const char *)&be64, sizeof be64);
}

///
/// Append int32_t using network endian
///
void buffer_append_int32(buffer *buf, int32_t x) {
	CHECK_VAILD_PTR(buf);
	int32_t be32 = hton32(x);
	buffer_append_bytes(buf, (const char *)&be32, sizeof be32);
}

void buffer_append_int16(buffer *buf, int16_t x) {
	CHECK_VAILD_PTR(buf);
	int16_t be16 = hton16(x);
	buffer_append_bytes(buf, (const char *)&be16, sizeof be16);
}

void buffer_append_int8(buffer *buf, int8_t x) {
	CHECK_VAILD_PTR(buf);
	int8_t be8 = x;
	buffer_append_bytes(buf, (const char *)&be8, sizeof be8);
}

///
/// Read int64_t from network endian
///
/// Require: buf->readableBytes() >= sizeof(int32_t)
int64_t buffer_read_int64(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	int64_t result = buffer_peek_int64(buf);
	buffer_retrieve_int64(buf);

	return result;
}

///
/// Read int32_t from network endian
///
/// Require: buf->readableBytes() >= sizeof(int32_t)
int32_t buffer_read_int32(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	int32_t result = buffer_peek_int32(buf);
	buffer_retrieve_int32(buf);

	return result;
}

int16_t buffer_read_int16(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	int16_t result = buffer_peek_int16(buf);
	buffer_retrieve_int16(buf);

	return result;
}

int8_t buffer_read_int8(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	int8_t result = buffer_peek_int8(buf);
	buffer_retrieve_int8(buf);

	return result;
}

///
/// Peek int64_t from network endian
///
/// Require: buf->readableBytes() >= sizeof(int64_t)
int64_t buffer_peek_int64(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
    CHECK(buffer_get_readable(buf) >= sizeof(int64_t));
    int64_t be64 = 0;
    memcpy(&be64, buffer_peek(buf), sizeof be64);

    return ntoh64(be64);
}

///
/// Peek int32_t from network endian
///
/// Require: buf->readableBytes() >= sizeof(int32_t)
int32_t buffer_peek_int32(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
    CHECK(buffer_get_readable(buf) >= sizeof(int32_t));
    int32_t be32 = 0;
    memcpy(&be32, buffer_peek(buf), sizeof be32);

    return ntoh32(be32);
}

int16_t buffer_peek_int16(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
    CHECK(buffer_get_readable(buf) >= sizeof(int16_t));
    int16_t be16 = 0;
    memcpy(&be16, buffer_peek(buf), sizeof be16);

    return ntoh16(be16);
}

int8_t buffer_peek_int8(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
    CHECK(buffer_get_readable(buf) >= sizeof(int8_t));
    int8_t be8 = 0;
    memcpy(&be8, buffer_peek(buf), sizeof be8);

    return be8;
}

void buffer_prepend(buffer *buf, const char *data, size_t len) {
	CHECK_VAILD_PTR(buf);
	CHECK(len <= buffer_get_prependable(buf));
    buf->readerIndex -= len;
    memcpy(inner_buffer_begin(buf) + buf->readerIndex, data, len);
}

///
/// Prepend int64_t using network endian
///
void buffer_prepend_int64(buffer *buf, int64_t x) {
	CHECK_VAILD_PTR(buf);
	int64_t be64 = hton64(x);
	buffer_prepend(buf, (const char *)&be64, sizeof be64);
}

///
/// Prepend int32_t using network endian
///
void buffer_prepend_int32(buffer *buf, int32_t x) {
	CHECK_VAILD_PTR(buf);
	int32_t be32 = hton32(x);
	buffer_prepend(buf, (const char *)&be32, sizeof be32);
}

void buffer_prepend_int16(buffer *buf, int16_t x) {
	CHECK_VAILD_PTR(buf);
	int16_t be16 = hton16(x);
	buffer_prepend(buf, (const char *)&be16, sizeof be16);
}

void buffer_prepend_int8(buffer *buf, int8_t x) {
	CHECK_VAILD_PTR(buf);
	int8_t be8 = x;
	buffer_prepend(buf, (const char *)&be8, sizeof be8);
}

void buffer_shrink(buffer *buf, size_t reserve) {
	CHECK_VAILD_PTR(buf);
	buffer *other = buffer_create(0);
	buffer_ensure_writable(other, buffer_get_readable(buf) + reserve);
	buffer_append_bytes(other, buffer_peek(buf), buffer_get_readable(buf));

	buffer_swap(buf, other);
}

buffer *buffer_steal(buffer *buf) {
	size_t size = buffer_get_capacity(buf);
	buffer *newbuf = buffer_create(size);
	if(TEST_VAILD_PTR(newbuf)) {
		byte_array_resize(buf->buf, size);
		buffer_swap(buf, newbuf);
	}

	return newbuf;
}

size_t buffer_get_capacity(buffer *buf)  {
	CHECK_VAILD_PTR(buf);
	return byte_array_capacity(buf->buf);
}

/// Read data directly into buffer.
///
/// It may implement with readv(2)
/// @return result of read(2), @c errno is saved
size_t buffer_read_fromfd(buffer *buf, int fd, int* savedErrno) {
	CHECK_VAILD_PTR(buf);
	// saved an ioctl()/FIONREAD call to tell how much to read
	char extrabuf[65536];
	struct iovec vec[2];
	const size_t writable = buffer_get_writable(buf);
	vec[0].iov_base = inner_buffer_begin(buf) + buf->writerIndex;
	vec[0].iov_len = writable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof extrabuf;
	// when there is enough space in this buffer, don't read into extrabuf.
	// when extrabuf is used, we read 128k-1 bytes at most.
	const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;

	const ssize_t n = readv(fd, vec, iovcnt);
	if (n < 0) {
		*savedErrno = errno;
	} else if (n <= writable) {
		buf->writerIndex += n;
	} else {
		buf->writerIndex = byte_array_capacity(buf->buf);
		buffer_append_bytes(buf, extrabuf, n - writable);
	}

	// if (n == writable + sizeof extrabuf)
	// {
	//   goto line_30;
	// }
	return n;
}

