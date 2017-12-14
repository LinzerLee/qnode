/*
 * util.h
 *
 *  Created on: 2017年10月21日
 *      Author: linzer
 */

#ifndef __QNODE_UTIL_H__
#define __QNODE_UTIL_H__

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <zlib.h>

#include <define.h>
#include <stringpiece.h>
#include <errcode.h>

typedef struct {
	gzFile file;
} gzfile;

static inline gzfile gzfile_invaild() {
	gzfile gz = { NULL };
	return gz;
}

static inline int gzfile_set_buf_size(gzfile gz, int size) {
	return gzbuffer(gz.file, size) == 0;
}

static inline int gzfile_check_vaild(gzfile gz) {
	return gz.file != NULL;
}

static inline int gzfile_read(gzfile gz, void* buf, int len) {
	return gzread(gz.file, buf, len);
}

static inline int gzfile_write(gzfile gz, stringpiece *strpie) {
	return gzwrite(gz.file, stringpiece_to_cstring(strpie), stringpiece_size(strpie));
}

static inline off_t gzfile_tell(gzfile gz) {
	return gztell(gz.file);
}

#if ZLIB_VERNUM >= 0x1240
// number of compressed bytes
static inline off_t gzfile_offset(gzfile gz) {
	return gzoffset(gz.file);
}
#endif /* ZLIB_VERNUM >= 0x1240 */

static inline int gzfile_flush(gzfile gz, int f) {
	return gzflush(gz.file, f);
}

static inline gzfile gzfile_read_open(const char *filename) {
	gzfile gz = gzfile_invaild();
	gz.file = gzopen(filename, "rbe");
	return gz;
}

static inline gzfile gzfile_append_open(const char *filename) {
	gzfile gz = gzfile_invaild();
	gz.file = gzopen(filename, "abe");
	return gz;
}

static inline gzfile gzfile_write_exec_open(const char *filename) {
	gzfile gz = gzfile_invaild();
	gz.file = gzopen(filename, "wbxe");
	return gz;
}

static inline gzfile gzfile_trunc_open(const char *filename) {
	gzfile gz = gzfile_invaild();
	gz.file = gzopen(filename, "wbe");
	return gz;
}

static inline void gzfile_close(gzfile gz) {
	gzclose(gz.file);
}

#define SMALL_FILE_SIZE	(64 * 1024)

typedef struct {
	int fd;
	int errcode;
	int64_t filesize;
	int64_t modify;
	int64_t create;
	int64_t bufsize;
	char buf[SMALL_FILE_SIZE];
} smallfile;

static inline smallfile *smallfile_open(const char *filename) {
	assert(filename != NULL);
	smallfile *sf = (smallfile *)malloc(sizeof(smallfile));
	if(sf) {
		struct stat statbuf;
		sf->errcode = ERROR_SUCCESS;
		sf->bufsize = 0;
		sf->fd = open(filename, O_RDONLY | O_CLOEXEC);
		if(!sf->fd) {
			sf->errcode = ERROR_OPEN_FAILD;
		}

		if (fstat(sf->fd, &statbuf) == 0) {
			if (S_ISREG(statbuf.st_mode)) {
				sf->filesize = statbuf.st_size;
			} else if (S_ISDIR(statbuf.st_mode)) {
				sf->errcode = ERROR_EISDIR;
			}

			sf->modify = statbuf.st_mtime;
			sf->create = statbuf.st_ctime;
		} else {
			sf->errcode = ERROR_FAILD;
		}
	}

	return sf;
}

static inline int smallfile_read(smallfile *sf) {
	assert(sf != NULL);
	if(sf->fd >= 0) {
		ssize_t n = pread(sf->fd, sf->buf, sizeof(sf->buf) - 1, 0);
		if (n >= 0) {
			sf->bufsize = n;
			sf->buf[n] = '\0';
		} else {
			sf->errcode = errno;
		}
	}

	return sf->errcode;
}

static inline int smallfile_read_tostring(smallfile *sf, stringpiece *strpie) {
	assert(sf != NULL);
	assert(strpie != NULL);
	stringpiece_clear(strpie);
	ssize_t recv = 0;

	sf->errcode = ERROR_SUCCESS;
	do {
		recv = read(sf->fd, sf->buf, sizeof(sf->buf) - 1);
		if (recv > 0) {
			sf->buf[recv - 1] = '\0';
			stringpiece_append(strpie, sf->buf);
		} else if (recv < 0) {
			sf->errcode = errno;
		}
	} while(recv > 0);

	return sf->errcode;
}

static inline const char *smallfile_buffer(smallfile *sf) {
	assert(sf != NULL);
	return sf->buf;
}

static inline void smallfile_close(smallfile *sf) {
	if(sf) {
		if (sf->fd >= 0)
			close(sf->fd);
		free(sf);
	}
}

#define SMALL_BUFFER_SIZE	(64 * 1024)

typedef struct {
	FILE* fp;
	char buf[SMALL_BUFFER_SIZE];
	size_t writeBytes;
	int errcode;
} appendfile;

static inline appendfile *appendfile_open(const char *filename) {
	assert(filename != NULL);
	appendfile *af = (appendfile *)malloc(sizeof(appendfile));
	if(af) {
		af->errcode = ERROR_SUCCESS;
		af->fp = fopen(filename, "ae");
		if(!af->fp) {
			af->errcode = ERROR_OPEN_FAILD;
		} else {
			af->writeBytes = 0;
			setbuffer(af->fp, af->buf, sizeof af->buf);
		}
	}

	return af;
}

static inline void appendfile_close(appendfile *af) {
	assert(af != NULL);
	fclose(af->fp);
	free(af);
}

static inline size_t
appendfile_write(appendfile *af, const char* logline, size_t len) {
	assert(af != NULL);
	return fwrite(logline, 1, len, af->fp);
}

static inline void
appendfile_append(appendfile *af, const char* logline, const size_t len) {
	assert(af != NULL);
	size_t n = appendfile_write(af, logline, len);

	size_t remain = len - n;
	while (remain > 0) {
		size_t x = appendfile_write(af, logline + n, remain);
		if (x == 0) {
			af->errcode = ferror(af->fp);
			if (af->errcode) {
				fprintf(stderr, "appendfile append failed %s\n", strerror_tl(af->errcode));
			}
			break;
		}
		n += x;
		remain = len - n; // remain -= x
	}

	af->writeBytes += len;
}

static inline void appendfile_flush(appendfile *af) {
	assert(af != NULL);
	fflush(af->fp);
}

#endif /* __QNODE_UTIL_H__ */
