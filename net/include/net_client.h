/*
 * net_client.h
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#ifndef __QNODE_NET_CLIENT_H__
#define __QNODE_NET_CLIENT_H__

#include <define.h>

FORWARD_DECLAR(net_client)
FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_address)
FORWARD_DECLAR(net_connection)

net_client *client_create(net_eventloop* loop, const net_address serveraddr, const char *name);
void client_destroy(net_client **client);
net_connection *client_get_connection(net_client *client);
net_eventloop *client_get_eventloop(net_client *client);
bool client_test_retry(net_client *client);
void client_enable_retry(net_client *client);
const char *client_get_name(net_client *client);
void client_set_connect_cb(net_client *client, connect_callback connect_cb);
void client_set_message_cb(net_client *client, message_callback message_cb);
void client_set_writecomplete_cb(net_client *client, writecomplete_callback writecomplete_cb);
void client_connect(net_client *client);
void client_disconnect(net_client *client);
void client_stop(net_client *client);

#endif /* __QNODE_CLIENT_H__ */
