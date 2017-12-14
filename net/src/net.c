/*
 * net.c
 *
 *  Created on: 2017年10月25日
 *      Author: linzer
 */

#include <arpa/inet.h>
#include <fcntl.h>

#include <net.h>

uint64_t hton64(uint64_t host64) {
	return htonll(host64);
}

uint32_t hton32(uint32_t host32) {
	return htonl(host32);
}

uint16_t hton16(uint16_t host16) {
	return htons(host16);
}

uint64_t ntoh64(uint64_t net64) {
	return ntohll(net64);
}

uint32_t ntoh32(uint32_t net32) {
	return ntohl(net32);
}

uint16_t ntoh16(uint16_t net16) {
	return ntohs(net16);
}

void noblocking(int fd) {
	// non-block
	int flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
}

void closeonexe(int fd) {
	// close-on-exec
	int flags = fcntl(fd, F_GETFD, 0);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);
}

