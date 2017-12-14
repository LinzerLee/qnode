/*
 * net_connector.c
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#include <errno.h>

#include <net_address.h>
#include <net_channel.h>
#include <net_acceptor.h>
#include <net_eventloop.h>
#include <net_connector.h>


#define DISCONNECTED		0
#define CONNECTING		1
#define CONNECTED		2

static const int MAX_RETRY_DELAY_MS = (30 * 1000);
static const int INIT_RETRY_DELAY_MS = 500;

typedef struct net_connector {
	net_eventloop *loop;
	net_address serverAddr;
	atomic_t connect;			/* bool */
	atomic_t state;				/* enum */
	net_channel *channel;
	client_newconnection_entry entry;
	int retryDelayMS;
} net_connector;

net_connector *connector_create(net_eventloop *loop, net_address svraddr) {
	CHECK_VAILD_PTR(loop);
	MALLOC_DEF(connector, net_connector);
	if(TEST_VAILD_PTR(connector)) {
		connector->loop = loop;
		connector->serverAddr = svraddr;
		atomic_set(&connector->connect, false);
		atomic_set(&connector->state, DISCONNECTED);
		connector->retryDelayMS = INIT_RETRY_DELAY_MS;
		NUL(connector->channel);
		NUL(connector->entry.args);
		NUL(connector->entry.callback);
	}

	return connector;
}

void connector_destroy(net_connector **connector) {
	if(TEST_VAILD_PTR(connector) &&
			TEST_VAILD_PTR(*connector)) {
		CHECK(!(*connector)->channel);
	}
}

static inline void inner_stop_pending(void *args) {
	net_connector *connector = (net_connector *)args;
	CHECK_VAILD_PTR(connector);
	eventloop_check_inloopthread(connector->loop);
	if(atomic_get(&connector->state) == CONNECTING) {
		atomic_set(&connector->state, DISCONNECTED);
		net_socket sock = connector_removeandreset_channel(connector);
		connector_retry(connector, &sock);
	}
}

void connector_stop(net_connector *connector) {
	CHECK_VAILD_PTR(connector);
	atomic_set(&connector->connect, false);
	pending_entry entry;
	entry.args = connector;
	entry.callback = inner_stop_pending;
	eventloop_pending_func(connector->loop, entry);
}

static inline void inner_start_pending(void *args) {
	net_connector *connector = (net_connector *)args;
	CHECK_VAILD_PTR(connector);
	eventloop_check_inloopthread(connector->loop);
	CHECK(atomic_get(&connector->state) == DISCONNECTED);
	if(atomic_get(&connector->connect)) {
		connector_connect(connector);
	} else {
		printf("do not connect\n");
	}
}

void connector_retry(net_connector *connector, net_socket *sock) {
	CHECK_VAILD_PTR(connector);
	socket_close(sock);
	atomic_set(&connector->state, DISCONNECTED);
	if (atomic_get(&connector->connect)) {
		printf("Connector Retry\n");
		pending_entry entry;
		entry.args = connector;
		entry.callback = inner_start_pending;
		eventloop_settimer_after(connector->loop, connector->retryDelayMS / 1000.0, entry);
		connector->retryDelayMS = MIN(connector->retryDelayMS * 2, MAX_RETRY_DELAY_MS);
	} else {
		printf("Don't connect\n");
	}
}

static inline void inner_write_handle(void *args, timestamp ts) {
	net_connector *connector = (net_connector *)args;
	CHECK_VAILD_PTR(connector);

	if (atomic_get(&connector->state) == CONNECTING) {
		net_socket sock = connector_removeandreset_channel(connector);
		int err = socket_get_error(&sock);
		if (err) {
			printf("Handle Write error(%d) : %s\n", err, strerror_tl(err));
			connector_retry(connector, &sock);
		} else if (socket_test_selfconnect(&sock)) {
			printf("Handle Write self connect\n");
			connector_retry(connector, &sock);
		} else {
			atomic_set(&connector->state, CONNECTED);
			if (atomic_get(&connector->connect)) {
				connector->entry.callback(connector->entry.args, sock);
			} else {
				socket_close(&sock);
			}
		}
	} else {
		// what happened?
		printf("Handle Write what happened?\n");
		CHECK(atomic_get(&connector->state) == DISCONNECTED);
	}
}

static inline void inner_error_handle(void *args, timestamp ts) {
	// LOG_ERROR << "Connector::handleError state=" << state_;
	net_connector *connector = (net_connector *)args;
	CHECK_VAILD_PTR(connector);

	if (atomic_get(&connector->state) == CONNECTING) {
		net_socket sock = connector_removeandreset_channel(connector);
		// int err = socket_get_error(&sock);
		// LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
		connector_retry(connector, &sock);
	}
}

void connector_connecting(net_connector *connector, net_socket *sock) {
	CHECK_VAILD_PTR(connector);
	CHECK_VAILD_PTR(sock);
	atomic_set(&connector->state, CONNECTING);
	CHECK(!connector->channel);
	connector->channel = channel_create(connector->loop, socket_fd(sock));
	CHECK_VAILD_PTR(connector->channel);
	channel_event_entry entry;
	entry.args = connector;
	entry.callback = inner_write_handle;
	channel_set_writeentry(connector->channel, entry);
	entry.args = connector;
	entry.callback = inner_error_handle;
	channel_set_errorentry(connector->channel, entry);
	channel_enable_write(connector->channel);
}

void connector_connect(net_connector *connector) {
	CHECK_VAILD_PTR(connector);
	net_socket sock = socket_open(netaddr_family(&connector->serverAddr));
	int ret = socket_connect(&sock, &connector->serverAddr);
	int savedErrno = (ret == 0) ? 0 : errno;
	switch (savedErrno) {
		case 0:
		case EINPROGRESS:
		case EINTR:
		case EISCONN:
			connector_connecting(connector, &sock);
			break;
		case EAGAIN:
		case EADDRINUSE:
		case EADDRNOTAVAIL:
		case ECONNREFUSED:
		case ENETUNREACH:
			connector_retry(connector, &sock);
			break;
		case EACCES:
		case EPERM:
		case EAFNOSUPPORT:
		case EALREADY:
		case EBADF:
		case EFAULT:
		case ENOTSOCK:
			printf("connect error in connector_connect, errno %d\n", savedErrno);
			socket_close(&sock);
			break;
		default:
			printf("unexpected error in connector_connect, errno %d\n", savedErrno);
			break;
  }
}

void connector_start(net_connector *connector) {
	CHECK_VAILD_PTR(connector);
	atomic_set(&connector->connect, true);
	pending_entry entry;
	entry.args = connector;
	entry.callback = inner_start_pending;
	eventloop_run_pending(connector->loop, entry);
}

void connector_restart(net_connector *connector) {
	CHECK_VAILD_PTR(connector);
	eventloop_check_inloopthread(connector->loop);
	atomic_set(&connector->state, CONNECTED);
	connector->retryDelayMS = INIT_RETRY_DELAY_MS;
	atomic_set(&connector->connect, true);
	inner_start_pending(connector);
}

void connector_set_newconnentry(net_connector *connector, client_newconnection_entry entry) {
	CHECK_VAILD_PTR(connector);
	CHECK_VAILD_PTR(entry.callback);
	connector->entry = entry;
}

void connector_reset(void *args) {
	net_connector *connector = args;
	CHECK_VAILD_PTR(connector);
	NUL(connector->channel);
}

net_socket connector_removeandreset_channel(net_connector *connector) {
	CHECK_VAILD_PTR(connector);
	channel_disable_all(connector->channel);
	channel_remove(connector->channel);
	int sockfd = channel_get_fd(connector->channel);
	net_socket sock = { sockfd };
	// Can't reset channel_ here, because we are inside Channel::handleEvent
	pending_entry entry;
	entry.args = connector;
	entry.callback = connector_reset;
	eventloop_pending_func(connector->loop, entry);  // FIXME: unsafe

	return sock;
}

