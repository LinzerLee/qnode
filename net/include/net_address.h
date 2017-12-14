/*
 * net_address.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_ADDRESS_H__
#define __QNODE_NET_ADDRESS_H__

#include <stdint.h>
#include <netinet/in.h>

#include <stringpiece.h>

#define NET_ADDR_ANY			INADDR_ANY
#define NET_ADDR_LOOPBACK	INADDR_LOOPBACK
#define NET_ADDR6_ANY		in6addr_any
#define NET_ADDR6_LOOPBACK	in6addr_loopback

#define NET_AF				AF_INET
#define NET_AF6				AF_INET6

typedef sa_family_t net_family;

typedef struct net_address{
	union {
		struct sockaddr_in addr;
		struct sockaddr_in6 addr6;
	};
} net_address;

net_family netaddr_family(net_address *addr);
net_address netaddr4(uint16_t port, bool loopbackOnly);
net_address netaddr6(uint16_t port, bool loopbackOnly);
net_address cstr2netaddr(const char *ip, uint16_t port);
net_address cstr2netaddr6(const char *ip6, uint16_t port);
const struct sockaddr *netaddr2sockaddr(net_address *add);
net_address sockaddr62netaddr6(struct sockaddr_in6 addr6);
stringpiece netaddr2strpie(net_address *add, bool ipOnly);
uint32_t netendian_ip(net_address *add);
uint16_t netendian_port(net_address *add);
uint32_t host_ip(net_address *add);
uint16_t host_port(net_address *add);
int hostname2netadd(const char *hostname, net_address* add);


#endif /* __QNODE_NET_ADDRESS_H__ */
