/*
 * net_socket.c
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */
#include <stddef.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>

#include <errcode.h>
#include <net.h>
#include <net_socket.h>

static inline void socket_noblocking(net_socket *sock) {
	assert(sock != NULL);
	noblocking(sock->sockfd);
}

static inline void socket_closeonexe(net_socket *sock) {
	assert(sock != NULL);
	closeonexe(sock->sockfd);
}

static inline net_socket socket_invaild() {
	net_socket sock;
	sock.sockfd = INVAILD_SOCKET;

	return sock;
}

net_socket socket_from_fd(int sockfd) {
	net_socket sock;
	sock.sockfd = sockfd;

	return sock;
}

bool socket_test_vaild(net_socket *sock) {
	return sock->sockfd != INVAILD_SOCKET;
}

int socket_fd(const net_socket *sock) {
	assert(sock != NULL);
	return sock->sockfd;
}

void socket_getopt(net_socket *sock, net_socket_option *info) {
	assert(sock != NULL);
	assert(info != NULL);
	socklen_t len = sizeof(*info);
	bzero(info, len);
	len = sizeof info->acceptconn;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_ACCEPTCONN, &info->acceptconn, &len);
	len = sizeof info->broadcast;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_BROADCAST, &info->broadcast, &len);
	len = sizeof info->debug;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_DEBUG, &info->debug, &len);
	len = sizeof info->dontroute;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_DONTROUTE, &info->dontroute, &len);
	len = sizeof info->error;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_ERROR, &info->error, &len);
	len = sizeof info->keepalive;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_KEEPALIVE, &info->keepalive, &len);
	len = sizeof info->linger;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_LINGER, &info->linger, &len);
	len = sizeof info->oobinline;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_OOBINLINE, &info->oobinline, &len);
	len = sizeof info->revbuf;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_RCVBUF, &info->revbuf, &len);
	len = sizeof info->revlowat;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_RCVLOWAT, &info->revlowat, &len);
	len = sizeof info->revtimeo;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_RCVTIMEO, &info->revtimeo, &len);
	len = sizeof info->reuseaddr;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_REUSEADDR, &info->reuseaddr, &len);
	len = sizeof info->reuseport;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_REUSEPORT, &info->reuseport, &len);
	len = sizeof info->sndbuf;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_SNDBUF, &info->sndbuf, &len);
	len = sizeof info->snflowat;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_SNDLOWAT, &info->snflowat, &len);
	len = sizeof info->sndtimeo;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_SNDTIMEO, &info->sndtimeo, &len);
	len = sizeof info->type;
	getsockopt(sock->sockfd, IPPROTO_TCP, SO_TYPE, &info->type, &len);
}

#ifdef __linux__
stringpiece socket_tcpinfo(net_socket *sock, struct tcp_info *info) {
	assert(sock != NULL);
	struct tcp_info info_b;
	struct tcp_info *tinfo = NULL;
	stringpiece strpie;
	if(!info) {
		tinfo = &info_b;
	} else {
		tinfo = info;
	}
	socklen_t len = sizeof(*tinfo);
	bzero(tinfo, len);
	getsockopt(sockfd_, SOL_TCP, TCP_INFO, tinfo, &len);
	char buf[1024];
	snprintf(buf, sizeof buf, "unrecovered=%u "
	             "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
	             "lost=%u retrans=%u rtt=%u rttvar=%u "
	             "sshthresh=%u cwnd=%u total_retrans=%u",
				 tinfo->tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
				 tinfo->tcpi_rto,          // Retransmit timeout in usec
				 tinfo->tcpi_ato,          // Predicted tick of soft clock in usec
				 tinfo->tcpi_snd_mss,
				 tinfo->tcpi_rcv_mss,
				 tinfo->tcpi_lost,         // Lost packets
				 tinfo->tcpi_retrans,      // Retransmitted packets out
				 tinfo->tcpi_rtt,          // Smoothed round trip time in usec
				 tinfo->tcpi_rttvar,       // Medium deviation
				 tinfo->tcpi_snd_ssthresh,
				 tinfo->tcpi_snd_cwnd,
				 tinfo->tcpi_total_retrans);  // Total retransmits for entire connection

	stringpiece_init_cstring(&strpie, buf);

	return  strpie;
}

#endif /* __linux__ */
#include <arpa/inet.h>
void socket_bind(net_socket *sock, net_address *addr) {
	CHECK_VAILD_PTR(sock);
	CHECK_VAILD_PTR(addr);
	int ret = bind(sock->sockfd, netaddr2sockaddr(addr), (socklen_t)(sizeof(struct sockaddr)));
	int saveErrno = errno;
	if (ret < 0) {
		stringpiece str = netaddr2strpie(addr, false);
		fprintf(stderr, "%s bind faild!(%d : %s)\n", stringpiece_to_cstring(&str), saveErrno, strerror_tl(saveErrno));
		stringpiece_release(&str);
		exit(ERROR_FAILD);
	}
}

void socket_listen(net_socket *sock) {
	assert(sock != NULL);
	int ret = listen(sock->sockfd, SOMAXCONN);
	if (ret < 0) {
		fprintf(stderr, "socket %d listen faild!\n", sock->sockfd);
		ABORT
	}
}

static inline int impl_accept(int sockfd, struct sockaddr_in6* addr) {
	socklen_t addrlen = (socklen_t)(sizeof *addr);
#if VALGRIND || defined (NO_ACCEPT4) || !defined(SOCK_NONBLOCK)
	int connfd = accept(sockfd, (struct sockaddr *)addr, &addrlen);
	noblocking(connfd);
	closeonexe(connfd);
#else
	int connfd = accept4(sockfd, (struct sockaddr *)addr, &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
  if (connfd < 0) {
    int savedErrno = errno;
    // LOG_SYSERR << "Socket::accept";
    printf("socket::accept\n");
    switch (savedErrno)
    {
      case EAGAIN:
      case ECONNABORTED:
      case EINTR:
      case EPROTO: // ???
      case EPERM:
      case EMFILE: // per-process lmit of open file desctiptor ???
    	  	  // expected errors
    	  	  errno = savedErrno;
    	  	  break;
      case EBADF:
      case EFAULT:
      case EINVAL:
      case ENFILE:
      case ENOBUFS:
      case ENOMEM:
      case ENOTSOCK:
      case EOPNOTSUPP:
    	  	  // unexpected errors
    	  	  // LOG_FATAL << "unexpected error of ::accept " << savedErrno;
    	  	  printf("unexpected error of accept\n");
    	  	  exit(ERROR_FAILD);
        break;
      default:
    	  	  // LOG_FATAL << "unknown error of ::accept " << savedErrno;
    	  	  printf("unknown error of accept\n");
    	  	  exit(ERROR_FAILD);
        break;
    }
  }

  return connfd;
}

net_socket socket_accept(net_socket *sock, net_address *netaddr) {
	assert(sock != NULL);
	assert(netaddr != NULL);
	struct sockaddr_in6 addr;
	net_socket peerSock;
	bzero(&addr, sizeof addr);
	int connfd = impl_accept(sock->sockfd, &addr);
	peerSock.sockfd = connfd;
	if (connfd >= 0) {
		*netaddr = sockaddr62netaddr6(addr);
	}

	return peerSock;
}

int socket_connect(net_socket *sock, const net_address *netaddr) {
	assert(sock != NULL);
	assert(netaddr != NULL);

	return connect(sock->sockfd, (struct sockaddr *)&netaddr->addr,
			(socklen_t)(sizeof(struct sockaddr_in)));
}

size_t socket_read(net_socket *sock, void *buf, size_t count) {
	assert(sock != NULL);
	assert(buf != NULL);

	return read(sock->sockfd, buf, count);
}

size_t socket_readv(net_socket *sock, const struct iovec *iov, int iovcnt) {
	assert(sock != NULL);
	assert(iov != NULL);

	return readv(sock->sockfd, iov, iovcnt);
}

size_t socket_write(net_socket *sock, const void *buf, size_t count) {
	assert(sock != NULL);
	assert(buf != NULL);

	return write(sock->sockfd, buf, count);
}

net_socket socket_open(net_family family) {
	net_socket sock;
	sock.sockfd = socket(family, SOCK_STREAM, IPPROTO_TCP);
	if(TEST_VAILD_SOCKET(sock.sockfd)) {
		socket_noblocking(&sock);
		socket_closeonexe(&sock);
	} else {
		sock = socket_invaild();
	}

	return sock;
}

void socket_close(net_socket *sock) {
	assert(sock != NULL);
	if (close(sock->sockfd) < 0) {
		// LOG_SYSERR << "sockets::close";
	}
}

void socket_shutdown_write(net_socket *sock) {
	assert(sock != NULL);
	printf("Shutdown Write\n");
	if (shutdown(sock->sockfd, SHUT_WR) < 0) {
		printf("shutdown write faild");
	}
}

void socket_tcpnodelay(net_socket *sock, bool on) {
	int optval = on ? 1 : 0;
	assert(sock != NULL);
	setsockopt(sock->sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

void socket_reuseaddr(net_socket *sock, bool on) {
	int optval = on ? 1 : 0;
	assert(sock != NULL);
	setsockopt(sock->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void socket_reuseport(net_socket *sock, bool on) {
#ifdef SO_REUSEPORT
	int optval = on ? 1 : 0;
	assert(sock != NULL);
	setsockopt(sock->sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
#else
	if (on) {
		// LOG_ERROR << "SO_REUSEPORT is not supported.";
	}
#endif
}

void socket_keepalive(net_socket *sock, bool on) {
	int optval = on ? 1 : 0;
	assert(sock != NULL);
	setsockopt(sock->sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}

int socket_get_error(net_socket *sock) {
	int optval;
	socklen_t optlen = (socklen_t)(sizeof optval);

	if (getsockopt(socket_fd(sock), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
	  return errno;
	} else {
	  return optval;
	}
}

net_address socket_get_localaddr(net_socket *sock) {
	struct sockaddr_in6 localaddr;
	bzero(&localaddr, sizeof localaddr);
	socklen_t addrlen = (socklen_t)(sizeof localaddr);
	if (getsockname(socket_fd(sock), (struct sockaddr *)(&localaddr), &addrlen) < 0) {
		// LOG_SYSERR << "sockets::getLocalAddr";
	}

	net_address netaddr;
	netaddr.addr6 = localaddr;
	return netaddr;
}

net_address socket_get_peeraddr(net_socket *sock) {
	struct sockaddr_in6 peeraddr;
	bzero(&peeraddr, sizeof peeraddr);
	socklen_t addrlen = (socklen_t)(sizeof peeraddr);
	if (getpeername(socket_fd(sock), (struct sockaddr *)(&peeraddr), &addrlen) < 0) {
		// LOG_SYSERR << "sockets::getPeerAddr";
	}

	net_address netaddr;
	netaddr.addr6 = peeraddr;
	return netaddr;
}

uint16_t socket_get_port(net_socket *sock) {
	CHECK_VAILD_PTR(sock);
	CHECK_VAILD_SOCKET(sock->sockfd);
	struct sockaddr_in addr;
	socklen_t len = sizeof addr;
	getsockname(sock->sockfd, (struct sockaddr*)&addr, &len);

	return ntoh16(addr.sin_port);
}
bool socket_test_selfconnect(net_socket *sock)
{
	struct sockaddr_in6 localaddr = socket_get_localaddr(sock).addr6;
	struct sockaddr_in6 peeraddr = socket_get_peeraddr(sock).addr6;
	if (localaddr.sin6_family == AF_INET) {
		const struct sockaddr_in* laddr4 = (struct sockaddr_in*)(&localaddr);
		const struct sockaddr_in* raddr4 = (struct sockaddr_in*)(&peeraddr);
		return laddr4->sin_port == raddr4->sin_port
				&& laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
	} else if (localaddr.sin6_family == AF_INET6) {
		return localaddr.sin6_port == peeraddr.sin6_port
			  	&& memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;
	} else {
		return false;
	}
}

