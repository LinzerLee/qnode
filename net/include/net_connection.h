/*
 * net_connection.h
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#ifndef __QNODE_NET_CONNECTION_H__
#define __QNODE_NET_CONNECTION_H__

#include <define.h>
#include <buffer.h>
#include <timestamp.h>

// FORWARD_DECLAR(buffer)

typedef void(* connect_callback)(void *args);
typedef void(* message_callback)(void *args, buffer* buf, timestamp ts);
typedef void(* writecomplete_callback)(void *args);
typedef void(* hightwatermark_callback)(void *args, size_t size);
typedef void(* close_callback)(void *args);

typedef struct {
	void *args;
	union {
		connect_callback connect_cb;
		message_callback message_cb;
		writecomplete_callback writecomplete_cb;
		hightwatermark_callback hightwatermark_cb;
		close_callback close_cb;
	};
} connection_event_entry;

FORWARD_DECLAR(net_connection)
FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_socket)
FORWARD_DECLAR(net_address)

net_connection *connection_create(net_eventloop* loop, const char *name,
		const net_socket *sock, const net_address *localaddr, const net_address* peeraddr);
void connection_destroy(net_connection **conn);
stringpiece connection_get_name(net_connection *conn);
net_socket connection_get_socket(net_connection *conn);
net_address connection_get_localaddr(net_connection *conn);
net_address connection_get_peeraddr(net_connection *conn);
bool connection_test_connected(net_connection *conn);
bool connection_test_connecting(net_connection *conn);
bool connection_test_disconnected(net_connection *conn);
bool connection_test_disconnecting(net_connection *conn);
void connection_set_connection_entry(net_connection *conn, connection_event_entry entry);
void connection_set_message_entry(net_connection *conn, connection_event_entry entry);
void connection_set_writecomplete_entry(net_connection *conn, connection_event_entry entry);
void connection_set_hwm_entry(net_connection *conn, connection_event_entry entry, size_t highWaterMark);
void connection_set_close_entry(net_connection *conn, connection_event_entry entry) ;
/// Advanced interface
buffer* connection_get_inputbuffer(net_connection *conn);
buffer* connection_get_outputbuffer(net_connection *conn);
net_eventloop *connection_get_eventloop(net_connection *conn);
#ifdef __linux__
stringpiece connection_get_tcpinfo(net_connection *conn, struct tcp_info* tcpi);
#endif /* __linux__ */
void connection_send_bytes(net_connection *conn, const void* data, int len);
void connection_send_strpie(net_connection *conn, stringpiece strpie);
void connection_send_buffer(net_connection *conn, buffer* buf);
void connection_shutdown(net_connection *conn);
void connection_forceclose(net_connection *conn);
void connection_forceclose_delay(net_connection *conn, double seconds);
void connection_set_tcpnodelay(net_connection *conn, bool on);
void connection_start_read(net_connection *conn);
void connection_stop_read(net_connection *conn);
void connection_connect_established(net_connection *conn);
void connection_connect_destroyed(net_connection *conn);

#endif /* __QNODE_NET_CONNECTION_H__ */
