/*
 * net_acceptor.h
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#ifndef __QNODE_NET_ACCEPTOR_H__
#define __QNODE_NET_ACCEPTOR_H__

#include <define.h>
#include <net_socket.h>
#include <net_address.h>

typedef void (*server_newconnection_callback)(void *args, net_socket sock, net_address);

typedef struct {
	server_newconnection_callback callback;
	void *args;
} server_newconnection_entry;

FORWARD_DECLAR(net_acceptor)
FORWARD_DECLAR(net_eventloop)

net_acceptor *acceptor_create(net_eventloop *loop, net_address listenaddr, bool reuseport);
void acceptor_destroy(net_acceptor **acceptor);
void acceptor_listen(net_acceptor *acceptor);
void acceptor_set_newconnentry(net_acceptor *acceptor, server_newconnection_entry entry);
net_eventloop *acceptor_get_eventloop(net_acceptor *acceptor);
net_socket acceptor_get_sockfd(net_acceptor *acceptor);

#endif /* __QNODE_NET_ACCEPTOR_H__ */
