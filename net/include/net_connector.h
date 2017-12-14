/*
 * net_connector.h
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#ifndef __QNODE_INCLUDE_NET_CONNECTOR_H__
#define __QNODE_INCLUDE_NET_CONNECTOR_H__

typedef void (*client_newconnection_callback)(void *args, net_socket sock);

typedef struct {
	client_newconnection_callback callback;
	void *args;
} client_newconnection_entry;

FORWARD_DECLAR(net_connector)
FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_socket)

net_connector *connector_create(net_eventloop *loop, net_address svraddr);
void connector_destroy(net_connector **connector);
void connector_stop(net_connector *connector);
void connector_retry(net_connector *connector, net_socket *sock);;
void connector_connecting(net_connector *connector, net_socket *sock);
void connector_connect(net_connector *connector);;
void connector_start(net_connector *connector);
void connector_restart(net_connector *connector);
void connector_set_newconnentry(net_connector *connector, client_newconnection_entry entry);
void connector_reset(void *args);
net_socket connector_removeandreset_channel(net_connector *connector);



#endif /* __QNODE_INCLUDE_NET_CONNECTOR_H__ */
