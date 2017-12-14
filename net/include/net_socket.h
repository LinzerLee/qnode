/*
 * net_socket.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_SOCKET_H__
#define __QNODE_NET_SOCKET_H__

#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <net_address.h>

typedef struct net_socket{
	int sockfd;
} net_socket;

typedef struct net_socket_option{
	int acceptconn;
	int broadcast;
	int debug;
	int dontroute;
	int error;
	int keepalive;
	struct linger linger;
	int oobinline;
	int revbuf;
	int revlowat;
	struct timeval revtimeo;
	int reuseaddr;
	int reuseport;
	int sndbuf;
	int snflowat;
	struct timeval sndtimeo;
	int type;
} net_socket_option;

struct sockaddr_in6;

net_socket socket_from_fd(int sockfd);
bool socket_test_vaild(net_socket *sock);
int socket_fd(const net_socket *sock);
void socket_getopt(net_socket *sock, net_socket_option *info);
#ifdef __linux__
stringpiece socket_tcpinfo(net_socket *sock, struct tcp_info *info);
#endif /* __linux__ */
void socket_bind(net_socket *sock, net_address *addr);
void socket_listen(net_socket *sock);
static inline int impl_accept(int sockfd, struct sockaddr_in6* addr);
net_socket socket_accept(net_socket *sock, net_address *netaddr);
int socket_connect(net_socket *sock, const net_address *netaddr);
size_t socket_read(net_socket *sock, void *buf, size_t count);
size_t socket_readv(net_socket *sock, const struct iovec *iov, int iovcnt);
size_t socket_write(net_socket *sock, const void *buf, size_t count);
net_socket socket_open(net_family family);
void socket_close(net_socket *sock);
void socket_shutdown_write(net_socket *sock);
void socket_tcpnodelay(net_socket *sock, bool on);
void socket_reuseaddr(net_socket *sock, bool on);
void socket_reuseport(net_socket *sock, bool on);
void socket_keepalive(net_socket *sock, bool on);
int socket_get_error(net_socket *sock) ;
net_address socket_get_localaddr(net_socket *sock);
net_address socket_get_peeraddr(net_socket *sock);
uint16_t socket_get_port(net_socket *sock);
bool socket_test_selfconnect(net_socket *sock);
#endif /* __QNODE_NET_SOCKET_H__ */
