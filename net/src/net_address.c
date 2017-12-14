/*
 * net_address.c
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>

#include <define.h>
#include <errcode.h>
#include <net.h>
#include <net_address.h>

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

static inline void sockaddr_init(const char* ip, uint16_t port, struct sockaddr_in* addr)	{
	addr->sin_family = AF_INET;
	addr->sin_port = hton16(port);
	if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0) {
		// LOG_SYSERR << "sockets::fromIpPort";
	}
}

static inline void sockaddr6_init(const char* ip, uint16_t port, struct sockaddr_in6* addr) {
	addr->sin6_family = AF_INET6;
	addr->sin6_port = hton16(port);
	if (inet_pton(AF_INET6, ip, &addr->sin6_addr) <= 0) {
		// LOG_SYSERR << "sockets::fromIpPort";
	}
}

static inline void sockaddr_to_ip_cstring(char* buf, size_t size, const struct sockaddr* addr) {
	if (addr->sa_family == AF_INET) {
		assert(size >= INET_ADDRSTRLEN);
		const struct sockaddr_in* addr4 = (struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &addr4->sin_addr, buf, (socklen_t)size);
	} else if (addr->sa_family == AF_INET6) {
		assert(size >= INET6_ADDRSTRLEN);
		const struct sockaddr_in6* addr6 = (struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &addr6->sin6_addr, buf, (socklen_t)size);
	}
}

static inline void
sockaddr_to_ip_port_cstring(char* buf, size_t size, const struct sockaddr* addr)	{
	sockaddr_to_ip_cstring(buf, size, addr);
	size_t end = strlen(buf);
	const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
	uint16_t port = ntohs(addr4->sin_port);
	assert(size > end);
	snprintf(buf+end, size-end, ":%u", port);
}

/* net address */
STATIC_ASSERT(sizeof(net_address) == sizeof(struct sockaddr_in6), 结构体netaddr的大小不等于sockaddr_in6);
STATIC_ASSERT(OFFSETOF(net_address, addr) == 0, 字段addr的偏移量在结构体net_address中不为0);
STATIC_ASSERT(OFFSETOF(net_address, addr6) == 0, 字段addr6的偏移量在结构体net_address中不为0);

net_family netaddr_family(net_address *addr) {
	return addr->addr.sin_family;
}

net_address netaddr4(uint16_t port, bool loopbackOnly) {
	net_address addr;
	bzero(&addr.addr, sizeof addr.addr);
	addr.addr.sin_family = NET_AF;
	in_addr_t ip = loopbackOnly ? NET_ADDR_LOOPBACK : NET_ADDR_ANY;
	addr.addr.sin_addr.s_addr = hton32(ip);
	addr.addr.sin_port = hton16(port);

	return addr;
}

net_address netaddr6(uint16_t port, bool loopbackOnly) {
	net_address addr;
	bzero(&addr.addr6, sizeof addr.addr6);
	addr.addr6.sin6_family = NET_AF6;
	struct in6_addr ip = loopbackOnly ? NET_ADDR6_LOOPBACK : NET_ADDR6_ANY;
	addr.addr6.sin6_addr = ip;
	addr.addr6.sin6_port = hton16(port);

	return addr;
}

net_address cstr2netaddr(const char *ip, uint16_t port) {
	net_address addr;
	assert(ip != NULL);
	bzero(&addr.addr, sizeof addr.addr);
	sockaddr_init(ip, port, &addr.addr);

	return addr;
}

net_address cstr2netaddr6(const char *ip6, uint16_t port) {
	net_address addr;
	assert(ip6 != NULL);
	bzero(&addr.addr6, sizeof addr.addr6);
	sockaddr6_init(ip6, port, &addr.addr6);

	return addr;
}

const struct sockaddr *netaddr2sockaddr(net_address *add) {
	assert(add != NULL);
	return (const struct sockaddr *)&add->addr6;
}

net_address sockaddr62netaddr6(struct sockaddr_in6 addr6) {
	net_address netadd;
	netadd.addr6 = addr6;

	return netadd;
}

stringpiece netaddr2strpie(net_address *add, bool ipOnly) {
	char buf[64] = "";
	stringpiece strpie;

	assert(add != NULL);
	if(ipOnly) {
		sockaddr_to_ip_cstring(buf, sizeof buf, netaddr2sockaddr(add));
	} else {
		sockaddr_to_ip_port_cstring(buf, sizeof buf, netaddr2sockaddr(add));
	}

	stringpiece_init_cstring(&strpie, buf);

	return strpie;
}

uint32_t netendian_ip(net_address *add) {
	assert(add != NULL);
	assert(netaddr_family(add) == AF_INET);
	return add->addr.sin_addr.s_addr;
}

uint16_t netendian_port(net_address *add) {
	assert(add != NULL);
	assert(netaddr_family(add) == AF_INET);
	return add->addr.sin_port;
}

uint32_t host_ip(net_address *add) {
	return ntohl(netendian_ip(add));
}

uint16_t host_port(net_address *add) {
	return ntohs(netendian_port(add));
}

#ifdef __linux__
__thread static char t_resolveBuffer[64 * 1024];
#endif

int hostname2netadd(const char *hostname, net_address* add) {
	assert(add != NULL);
	struct hostent hent;
	struct hostent* he = NULL;
	bzero(&hent, sizeof(hent));
#ifdef __linux__
	int herrno = 0;
	int ret = gethostbyname_r(hostname, &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
#else
	he = gethostbyname(hostname);
	int ret = 0;
#endif
	if (ret == 0 && he != NULL) {
		assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
		add->addr.sin_addr = *(struct in_addr *)(he->h_addr);

		return ERROR_SUCCESS;
	} else {
		if (ret) {
			// LOG_SYSERR << "InetAddress::resolve";
		}
		return ERROR_FAILD;
	}
}
