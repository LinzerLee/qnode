/*
 * net_connection.c
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */
#include <errno.h>

#include <atomic.h>
#include <stringpiece.h>
#include <timestamp.h>
#include <net_address.h>
#include <net_socket.h>
#include <net_channel.h>
#include <net_eventloop.h>
#include <net_connection.h>

#define DISCONNECTED		0
#define CONNECTING		1
#define CONNECTED		2
#define DISCONNECTING	3

typedef struct net_connection{
	net_eventloop* loop;
	stringpiece name;
	atomic_t state;			/* enum */
	net_socket sock;
	net_channel *channel;
	net_address localaddr;
	net_address peeraddr;
	connection_event_entry connStateEntry;
	connection_event_entry messageEntry;
	connection_event_entry writecomplEntry;
	connection_event_entry hwmEntry;
	connection_event_entry closeEntry;
	size_t highWaterMark;
	atomic_t reading;		/* bool */
	buffer *input;
	buffer *output;
} net_connection;

static inline void inner_default_connection_cb(void *args) {
	// LOG_TRACE << conn->localAddress().toIpPort() << " -> " << conn->peerAddress().toIpPort() << " is " << (conn->connected() ? "UP" : "DOWN");
	// do not call conn->forceClose(), because some users want to register message callback only.
}

static inline void inner_default_message_cb(void *args, buffer* buf, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(buf);
	buffer_retrieve_all(buf);
}

static inline void inner_impl_shutdown(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if(!channel_can_write(conn->channel)) {
		// we are not writing
		socket_shutdown_write(&conn->sock);
	}
}

static inline void inner_impl_handleclose(void *args, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	// LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
	CHECK(connection_test_connected(conn) || connection_test_disconnecting(conn));
	// we don't close fd, leave it to dtor, so we can find leaks easily.
	atomic_set(&conn->state, DISCONNECTED);
	channel_disable_all(conn->channel);
	channel_remove(conn->channel);
	ATOMIC_FALSE(conn->reading);

	/* 消除CLOSE_WAIT状态 */
	socket_close(&conn->sock);

	if(TEST_VAILD_PTR(conn->connStateEntry.connect_cb)) {
		conn->connStateEntry.connect_cb(conn->connStateEntry.args);
	}

	// must be the last line
	if(TEST_VAILD_PTR(conn->closeEntry.close_cb)) {
		conn->closeEntry.close_cb(conn->closeEntry.args);
	}
}

static inline void inner_impl_handleerror(void *args, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	net_socket sock = socket_from_fd(channel_get_fd(conn->channel));
	int err = socket_get_error(&sock);
	// LOG_ERROR << "TcpConnection::handleError [" << name_ << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

static inline void inner_impl_handleread(void *args, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	int savedErrno = 0;
	ssize_t n = buffer_read_fromfd(conn->input, channel_get_fd(conn->channel), &savedErrno);
	if (n > 0) {
		printf("socket %d recv %zd bytes data.\n", conn->sock.sockfd, n);
		conn->messageEntry.message_cb(conn->messageEntry.args, conn->input, ts);
	} else if (n == 0) {
		printf("I konw socket %d will be closed.\n", conn->sock.sockfd);
		inner_impl_handleclose(conn, ts);
	} else {
		errno = savedErrno;
		fprintf(stderr, "connection : chanel handle read event happend error(%d)!\n", savedErrno);
		ABORT
		inner_impl_handleerror(conn, ts);
	}
}

static inline void inner_impl_handlewrite(void *args, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if (channel_can_write(conn->channel)) {
		net_socket sock = socket_from_fd(channel_get_fd(conn->channel));
		ssize_t n = socket_write(&sock, buffer_peek(conn->output), buffer_get_readable(conn->output));
		if (n > 0) {
			buffer_retrieve(conn->output, n);
			if (buffer_get_readable(conn->output) == 0) {
				channel_disable_write(conn->channel);
				if (TEST_VAILD_PTR(conn->writecomplEntry.writecomplete_cb)) {
					pending_entry entry;
					entry.args = conn->writecomplEntry.args;
					entry.callback = conn->writecomplEntry.writecomplete_cb;
					eventloop_pending_func(conn->loop, entry);
				} if (connection_test_disconnecting(conn)) {
					inner_impl_shutdown(conn);
				}
			}
		} else {
			// LOG_SYSERR << "TcpConnection::handleWrite";
			// if (state_ == kDisconnecting) {
			  //   shutdownInLoop();
			// }
		}
	} else {
		// LOG_TRACE << "Connection fd = " << channel_->fd() << " is down, no more writing";
	}
}

net_connection *connection_create(net_eventloop* loop, const char *name, const net_socket *sock,
        const net_address *localaddr, const net_address* peeraddr) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(name);
	CHECK_VAILD_PTR(sock);
	CHECK_VAILD_PTR(localaddr);
	CHECK_VAILD_PTR(peeraddr);

	MALLOC_DEF(conn, net_connection);
	if(TEST_VAILD_PTR(conn)) {
		conn->loop = loop;
		stringpiece_init_cstring(&conn->name, name);
		atomic_set(&conn->state, CONNECTING);
		conn->sock = *sock;
		conn->localaddr = *localaddr;
		conn->peeraddr = *peeraddr;
		conn->highWaterMark = 64 * 1024 * 1024;
		ATOMIC_TRUE(conn->reading);
		conn->connStateEntry.args = conn;
		conn->connStateEntry.connect_cb = inner_default_connection_cb;
		conn->messageEntry.args = conn;
		conn->messageEntry.message_cb = inner_default_message_cb;
		NUL(conn->writecomplEntry.args);
		NUL(conn->writecomplEntry.writecomplete_cb);
		NUL(conn->hwmEntry.args);
		NUL(conn->hwmEntry.hightwatermark_cb);
		NUL(conn->closeEntry.args);
		NUL(conn->closeEntry.close_cb);
		conn->channel = channel_create(loop, socket_fd(sock));

		if(TEST_VAILD_PTR(conn->channel)) {
			channel_event_entry entry;
			entry.args = conn;
			entry.callback = inner_impl_handleread;
			channel_set_readentry(conn->channel, entry);
			entry.callback = inner_impl_handlewrite;
			channel_set_writeentry(conn->channel, entry);
			entry.callback = inner_impl_handleclose;
			channel_set_closeentry(conn->channel, entry);
			entry.callback = inner_impl_handleerror;
			channel_set_errorentry(conn->channel, entry);

			socket_keepalive(&conn->sock, true);
			conn->input = buffer_create(0);
			if(TEST_VAILD_PTR(conn->input)) {
				conn->output = buffer_create(0);
				if(TEST_VAILD_PTR(conn->output)) {
					return conn;
				}

				buffer_destroy(&conn->input);
			}

			channel_destroy(&conn->channel);
		}

		FREE(conn);
	}

	return conn;
}

void connection_destroy(net_connection **conn) {
	if(TEST_VAILD_PTR(conn) && TEST_VAILD_PTR(*conn)) {
		CHECK(atomic_get(&(*conn)->state) == DISCONNECTED);
		stringpiece_release(&(*conn)->name);
		channel_destroy(&(*conn)->channel);
		buffer_destroy(&(*conn)->input);
		buffer_destroy(&(*conn)->output);
	}
}

stringpiece connection_get_name(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return stringpiece_dup(&conn->name);
}

net_socket connection_get_socket(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->sock;
}

net_address connection_get_localaddr(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->localaddr;
}

net_address connection_get_peeraddr(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->peeraddr;
}

bool connection_test_connected(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return atomic_get(&conn->state) == CONNECTED;
}

bool connection_test_connecting(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return atomic_get(&conn->state) == CONNECTING;
}

bool connection_test_disconnected(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return atomic_get(&conn->state) == DISCONNECTED;
}

bool connection_test_disconnecting(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return atomic_get(&conn->state) == DISCONNECTING;
}

void connection_set_connection_entry(net_connection *conn, connection_event_entry entry) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(entry.connect_cb);
	conn->connStateEntry = entry;
}

void connection_set_message_entry(net_connection *conn, connection_event_entry entry) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(entry.message_cb);
	conn->messageEntry = entry;
}

void connection_set_writecomplete_entry(net_connection *conn, connection_event_entry entry) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(entry.writecomplete_cb);
	conn->writecomplEntry = entry;
}

void connection_set_hwm_entry(net_connection *conn, connection_event_entry entry, size_t highWaterMark) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(entry.hightwatermark_cb);
	conn->hwmEntry = entry;
	conn->highWaterMark = highWaterMark;
}

/// Internal use only.
void connection_set_close_entry(net_connection *conn, connection_event_entry entry) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(entry.close_cb);
	conn->closeEntry = entry;
}

/// Advanced interface
buffer* connection_get_inputbuffer(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->input;
}

buffer* connection_get_outputbuffer(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->output;
}

net_eventloop *connection_get_eventloop(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	return conn->loop;
}

#ifdef __linux__
stringpiece connection_get_tcpinfo(net_connection *conn, struct tcp_info* tcpi)  {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(tcpi);
	return socket_tcpinfo(&conn->sock, tcpi);
}
#endif /* __linux__ */

typedef struct {
	connection_event_entry hwmEntry;
	size_t size;
} inner_hwm2pending_entry;

static inline void inner_hwm2pending_entry_adapter(void *args) {
	inner_hwm2pending_entry *entry = (inner_hwm2pending_entry *)args;
	CHECK_VAILD_PTR(entry);
	entry->hwmEntry.hightwatermark_cb(entry->hwmEntry.args, entry->size);
	FREE(entry);
}

static inline void inner_impl_send(net_connection *conn, const void* data, size_t len) {
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	ssize_t nwrote = 0;
	size_t remaining = len;
	bool faultError = false;
	if (connection_test_connected(conn)) {
		// LOG_WARN << "disconnected, give up writing";
		return;
	}
	// if no thing in output queue, try writing directly
	if (!channel_can_write(conn->channel) && buffer_get_readable(conn->output) == 0) {
		net_socket sock = socket_from_fd(channel_get_fd(conn->channel));
		nwrote = socket_write(&sock, data, len);
		if (nwrote >= 0) {
			remaining = len - nwrote;
			if (remaining == 0 && TEST_VAILD_PTR(conn->writecomplEntry.writecomplete_cb)) {
				pending_entry entry;
				entry.args = conn->writecomplEntry.args;
				entry.callback = conn->writecomplEntry.writecomplete_cb;
				eventloop_pending_func(conn->loop, entry);
			}
		} else {
			// nwrote < 0
			nwrote = 0;
			if (errno != EWOULDBLOCK) {
				// LOG_SYSERR << "TcpConnection::sendInLoop";
				if (errno == EPIPE || errno == ECONNRESET)  {
					// FIXME: any others?
					faultError = true;
				}
			}
		}
	}

	assert(remaining <= len);
	if (!faultError && remaining > 0) {
		size_t oldLen = buffer_get_readable(conn->output);
		if (oldLen + remaining >= conn->highWaterMark && oldLen < conn->highWaterMark
				&& TEST_VAILD_PTR(conn->hwmEntry.hightwatermark_cb)) {
			MALLOC_DEF(inner_entry, inner_hwm2pending_entry);
			CHECK_VAILD_PTR(inner_entry);
			inner_entry->hwmEntry = conn->hwmEntry;
			inner_entry->size = oldLen + remaining;

			pending_entry entry;
			entry.args = inner_entry;
			entry.callback = inner_hwm2pending_entry_adapter;

			eventloop_pending_func(conn->loop, entry);
		}

		buffer_append_bytes(conn->output, data + nwrote, remaining);
		if(!channel_can_write(conn->channel)) {
			channel_enable_write(conn->channel);
		}
	}
}

typedef void (*implsendCB)(net_connection *conn, const void* data, size_t len);

typedef struct {
	implsendCB callback;
	net_connection *conn;
	const void* data;
	size_t len;
} inner_implsendCB2pending_entry;

static inline void inner_implsendCB2pending_entry_adapter(void *args) {
	inner_implsendCB2pending_entry *entry = (inner_implsendCB2pending_entry *)args;
	CHECK_VAILD_PTR(entry);
	entry->callback(entry->conn, entry->data, entry->len);
	FREE(entry);
}

void connection_send_bytes(net_connection *conn, const void* data, int len) {
	CHECK_VAILD_PTR(conn);
	CHECK_VAILD_PTR(data);

	if (connection_test_connected(conn)) {
		if (eventloop_test_inloopthread(conn->loop)) {
			inner_impl_send(conn, data, len);
		} else {
			MALLOC_DEF(inner_entry, inner_implsendCB2pending_entry);
			CHECK_VAILD_PTR(inner_entry);
			inner_entry->callback = inner_impl_send;
			inner_entry->conn = conn;
			inner_entry->data = data;
			inner_entry->len = len;
			pending_entry entry;
			entry.args = inner_entry;
			entry.callback = inner_implsendCB2pending_entry_adapter;
			eventloop_run_pending(conn->loop, entry);
		}
	}
}

void connection_send_strpie(net_connection *conn, stringpiece strpie) {
	CHECK_VAILD_PTR(conn);
	CHECK(stringpiece_size(&strpie) > 0);
	connection_send_bytes(conn, stringpiece_data(&strpie), stringpiece_size(&strpie));
}

void connection_send_buffer(net_connection *conn, buffer* buf) {
	CHECK_VAILD_PTR(conn);
	connection_send_bytes(conn, buffer_peek(buf), buffer_get_readable(buf));
	buffer_retrieve_all(buf);
}

void connection_shutdown(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	printf("connection shutdown\n");
	if (connection_test_connected(conn)) {
		atomic_set(&conn->state, DISCONNECTING);
		pending_entry entry;
		entry.args = conn;
		entry.callback = inner_impl_shutdown;
		eventloop_run_pending(conn->loop, entry);
	}
}

static inline void inner_forceclose(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if (connection_test_disconnected(conn) || connection_test_disconnecting(conn)) {
		// as if we received 0 byte in handleRead();
		inner_impl_handleclose(conn, timestamp_now());
	}
}

void connection_forceclose(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	if(connection_test_connected(conn) || connection_test_disconnecting(conn)) {
		atomic_set(&conn->state, DISCONNECTING);
		pending_entry entry;
		entry.args = conn;
		entry.callback = inner_forceclose;
		eventloop_pending_func(conn->loop, entry);
	}
}

void connection_forceclose_delay(net_connection *conn, double seconds) {
	CHECK_VAILD_PTR(conn);
	if (connection_test_connected(conn) || connection_test_disconnecting(conn)) {
		atomic_set(&conn->state, DISCONNECTING);
		pending_entry entry;
		entry.args = conn;
		entry.callback = inner_forceclose;
		eventloop_settimer_after(conn->loop, seconds, entry);
	}
}

void connection_set_tcpnodelay(net_connection *conn, bool on) {
	CHECK_VAILD_PTR(conn);
	socket_tcpnodelay(&conn->sock, on);
}

static inline void inner_imple_startread(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if(!ATOMIC_TEST(conn->reading) || !channel_can_read(conn->channel)) {
		channel_enable_read(conn->channel);
		ATOMIC_TRUE(conn->reading);
	}
}

void connection_start_read(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	pending_entry entry;
	entry.args = conn;
	entry.callback = inner_imple_startread;

	eventloop_run_pending(conn->loop, entry);
}

static inline void inner_imple_stopread(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if(ATOMIC_TEST(conn->reading) || channel_can_read(conn->channel)) {
		channel_disable_read(conn->channel);
		ATOMIC_FALSE(conn->reading);
	}
}

void connection_stop_read(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	pending_entry entry;
	entry.args = conn;
	entry.callback = inner_imple_stopread;

	eventloop_run_pending(conn->loop, entry);
}

void connection_connect_established(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	CHECK(connection_test_connecting(conn));
	atomic_set(&conn->state, CONNECTED);
	channel_tie(conn->channel, conn);
	channel_enable_read(conn->channel);
	if(TEST_VAILD_PTR(conn->connStateEntry.connect_cb)) {
		conn->connStateEntry.connect_cb(conn->connStateEntry.args);
	}
}

void connection_connect_destroyed(net_connection *conn) {
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(conn->loop);
	if (connection_test_connected(conn)) {
		atomic_set(&conn->state, DISCONNECTED);
		channel_disable_all(conn->channel);
		if(TEST_VAILD_PTR(conn->connStateEntry.connect_cb)) {
			conn->connStateEntry.connect_cb(conn->connStateEntry.args);
		}
	}

	channel_remove(conn->channel);
}
