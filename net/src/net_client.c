/*
 * net_client.c
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */



#include <atomic.h>
#include <mutex.h>
#include <net_socket.h>
#include <net_address.h>
#include <net_eventloop.h>
#include <net_connection.h>
#include <net_connector.h>
#include <net_client.h>

typedef struct net_client {
	net_eventloop *loop;
	net_connector *connector;
	char *name;
	connect_callback connect_cb;
	message_callback message_cb;
	writecomplete_callback writecomplete_cb;
	atomic_t retry;   		/* bool */
	atomic_t connect;   		/* bool */
	// always in loop thread
	int next_conn_id;
	mutex lock;
	net_connection *connection; // protected mutex lock
} net_client;

typedef void(* inner_connection_remove_cb)(net_client *client, net_connection *conn);

typedef struct {
	net_client *client;
	net_connection *conn;
	inner_connection_remove_cb callback;
} inner_connection_remove_entry;

static inline void connection_connect_destroyed_adpater(void *args) {
	net_connection *conn = (net_connection *)args;
	connection_connect_destroyed(conn);
}

static inline void inner_connection_remove(net_client *client, net_connection *conn) {
	CHECK_VAILD_PTR(client);
	CHECK_VAILD_PTR(conn);
	eventloop_check_inloopthread(client->loop);
	CHECK(client->loop == connection_get_eventloop(conn));
	CHECK(client->connection == conn);
	/*
	MUTEX_LOCK(client);
	NUL(client->connection);
	MUTEX_UNLOCK(client);
	*/
	pending_entry entry;
	entry.args = conn;
	entry.callback = connection_connect_destroyed_adpater;
	eventloop_run_pending(client->loop, entry);

	if (ATOMIC_TEST(client->retry) && ATOMIC_TEST(client->connect)) {
		// LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to " << connector_->serverAddress().toIpPort();
		connector_restart(client->connector);
	}
}

static inline void inner_connection_remove_adpater(void *args) {
	inner_connection_remove_entry *inner_entry = (inner_connection_remove_entry *)args;
	CHECK_VAILD_PTR(inner_entry);
	inner_entry->callback(inner_entry->client, inner_entry->conn);

	FREE(inner_entry);
}

static inline void inner_newconnection(void *args, net_socket sock) {
	net_client *client = (net_client *)args;
	CHECK_VAILD_PTR(client);
	eventloop_check_inloopthread(client->loop);
	net_address peeraddr = socket_get_peeraddr(&sock);
	stringpiece strpie = netaddr2strpie(&peeraddr, false);
	char buf[32];
	snprintf(buf, sizeof buf, ":%s#%d", stringpiece_to_cstring(&strpie), client->next_conn_id);

	++ client->next_conn_id;
	stringpiece_clear(&strpie);
	stringpiece_append(&strpie, client->name);
	stringpiece_append(&strpie, buf);
	net_address localaddr = socket_get_localaddr(&sock);
	net_connection *conn = connection_create(client->loop, stringpiece_to_cstring(&strpie), &sock, &localaddr, &peeraddr);
	if(TEST_VAILD_PTR(conn)) {
		MUTEX_LOCK(client);
		client->connection = conn;
		MUTEX_UNLOCK(client);
		connection_event_entry entry;
		entry.args = client->connection;
		if(TEST_VAILD_PTR(client->connection)) {
			entry.connect_cb = client->connect_cb;
			connection_set_connection_entry(client->connection, entry);
		}

		if(TEST_VAILD_PTR(client->message_cb)) {
			entry.message_cb = client->message_cb;
			connection_set_message_entry(client->connection, entry);
		}

		if(TEST_VAILD_PTR(client->writecomplete_cb)) {
			entry.writecomplete_cb = client->writecomplete_cb;
			connection_set_writecomplete_entry(client->connection, entry);
		}

		MALLOC_DEF(inner_entry, inner_connection_remove_entry);
		inner_entry->callback = inner_connection_remove;
		inner_entry->client = client;
		inner_entry->conn = client->connection;
		entry.args = inner_entry;
		entry.close_cb = inner_connection_remove_adpater;
		connection_set_close_entry(client->connection, entry);
		connection_connect_established(client->connection);
	} else {

	}
}

net_client *client_create(net_eventloop* loop, const net_address serveraddr, const char *name) {
	CHECK_VAILD_PTR(loop);
	MALLOC_DEF(client, net_client);
	if(TEST_VAILD_PTR(client)) {
		client->loop = loop;
		client->name = strdup(name);
		NUL(client->connect_cb);
		NUL(client->message_cb);
		NUL(client->writecomplete_cb);
		ATOMIC_FALSE(client->retry);
		ATOMIC_FALSE(client->connect);
		MUTEX_INIT(client);
		client->next_conn_id = 1;
		client->connector = connector_create(loop, serveraddr);
		NUL(client->connection);
		if(TEST_VAILD_PTR(client->connector)) {
			client_newconnection_entry entry;
			entry.args = client;
			entry.callback = inner_newconnection;
			connector_set_newconnentry(client->connector, entry);

			return client;
		}

		FREE(client);
	}

	return client;
}

typedef void(* inner_removeconn_cb)(net_eventloop *loop, net_connection *conn);

typedef struct {
	net_eventloop *loop;
	net_connection *conn;
	inner_removeconn_cb callback;
} inner_removeconn_entry;

static inline void inner_remove_connection_adapter(void *args) {
	inner_removeconn_entry *inner_entry = (inner_removeconn_entry *)args;
	CHECK_VAILD_PTR(inner_entry);
	inner_entry->callback(inner_entry->loop, inner_entry->conn);
	FREE(inner_entry);
}

static inline void
inner_remove_connection(net_eventloop *loop, net_connection *conn) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(conn);
	pending_entry entry;
	entry.args = conn;
	entry.callback = connection_connect_destroyed_adpater;
	eventloop_run_pending(loop, entry);
}

void client_destroy(net_client **client) {
	if(TEST_VAILD_PTR(client) && TEST_VAILD_PTR(*client)) {
		CHECK((*client)->loop == connection_get_eventloop((*client)->connection));
		// FIXME: not 100% safe, if we are in different thread
		MALLOC_DEF(inner_entry, inner_removeconn_entry);
		inner_entry->loop = (*client)->loop;
		inner_entry->conn = (*client)->connection;
		inner_entry->callback = inner_remove_connection;
		pending_entry entry;
		entry.args = inner_entry;
		entry.callback = inner_remove_connection_adapter;

		eventloop_run_pending((*client)->loop, entry);
		connection_forceclose((*client)->connection);

		connector_destroy(&(*client)->connector);
		MUTEX_DESTROY(*client);
		FREE((*client)->name);
		FREE(*client);
	}
}

net_connection *client_get_connection(net_client *client) {
	CHECK_VAILD_PTR(client);
	return client->connection;
}

net_eventloop *client_get_eventloop(net_client *client) {
	CHECK_VAILD_PTR(client);
	return client->loop;
}

bool client_test_retry(net_client *client) {
	CHECK_VAILD_PTR(client);
	return ATOMIC_TEST(client->retry);
}

void client_enable_retry(net_client *client) {
	CHECK_VAILD_PTR(client);
	ATOMIC_TRUE(client->retry);
}

const char *client_get_name(net_client *client) {
	CHECK_VAILD_PTR(client);
	return client->name;
}

/// Set connection callback.
/// Not thread safe.
void client_set_connect_cb(net_client *client, connect_callback connect_cb) {
	CHECK_VAILD_PTR(client);
	client->connect_cb = connect_cb;
}

/// Set message callback.
/// Not thread safe.
void client_set_message_cb(net_client *client, message_callback message_cb) {
	CHECK_VAILD_PTR(client);
	client->message_cb = message_cb;
}

/// Set write complete callback.
/// Not thread safe.
void client_set_writecomplete_cb(net_client *client, writecomplete_callback writecomplete_cb) {
	CHECK_VAILD_PTR(client);
	client->writecomplete_cb = writecomplete_cb;
}

void client_connect(net_client *client) {
	CHECK_VAILD_PTR(client);
	// FIXME: check state
	// LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to " << connector_->serverAddress().toIpPort();

	ATOMIC_TRUE(client->connect);
	connector_start(client->connector);
}

void client_disconnect(net_client *client) {
	CHECK_VAILD_PTR(client);
	ATOMIC_FALSE(client->connect);
	MUTEX_LOCK(client);
	if(TEST_VAILD_PTR(client->connection)) {
		connection_shutdown(client->connection);
	}
	MUTEX_UNLOCK(client);
}

void client_stop(net_client *client) {
	CHECK_VAILD_PTR(client);
	ATOMIC_FALSE(client->connect);
	connector_stop(client->connector);
}

