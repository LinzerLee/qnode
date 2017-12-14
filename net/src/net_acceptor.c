/*
 * net_acceptor.c
 *
 *  Created on: 2017年10月29日
 *      Author: linzer
 */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <atomic.h>
#include <define.h>
#include <timestamp.h>
#include <net_socket.h>
#include <net_address.h>
#include <net_channel.h>
#include <net_eventloop.h>
#include <net_acceptor.h>

typedef struct net_acceptor {
	net_eventloop *loop;
	net_socket socket;
	net_channel *channel;
	server_newconnection_entry entry;
	atomic_t listenning;			/* bool */
	int idlefd;
} net_acceptor;

static inline void inner_read_handle(void *args, timestamp ts) {
	net_acceptor *acceptor = (net_acceptor *)args;
	CHECK_VAILD_PTR(acceptor);
	eventloop_check_inloopthread(acceptor->loop);
	net_address peer;
	net_socket conn = socket_accept(&acceptor->socket, &peer);
	if (TEST_VAILD_SOCKET(socket_fd(&conn))) {
	    if (TEST_VAILD_PTR(acceptor->entry.callback)) {
	    		acceptor->entry.callback(acceptor->entry.args, conn, peer);
	    } else {
	    		socket_close(&conn);
	    }
	} else {
		fprintf(stderr, "accpet socket is invaild!\n");
	    if (errno == EMFILE) {
	    		close(acceptor->idlefd);
	    		acceptor->idlefd = accept(acceptor->socket.sockfd, NULL, NULL);
	    		close(acceptor->idlefd);
	    		acceptor->idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);
	    }
	}
}

net_acceptor *acceptor_create(net_eventloop *loop, net_address listenaddr, bool reuseport) {
	CHECK_VAILD_PTR(loop);
	MALLOC_DEF(acceptor, net_acceptor);
	if(TEST_VAILD_PTR(acceptor)) {
		acceptor->loop = loop;
		atomic_set(&acceptor->listenning, false);
		acceptor->socket = socket_open(netaddr_family(&listenaddr));

		CHECK_VAILD_SOCKET(acceptor->socket.sockfd);
		acceptor->idlefd = open("/dev/null", O_RDONLY | O_CLOEXEC);

		if(socket_test_vaild(&acceptor->socket) &&
				TEST_VAILD_FD(acceptor->idlefd)) {
			// socket_reuseaddr(&acceptor->socket, true);
			// socket_reuseport(&acceptor->socket, reuseport);
			socket_bind(&acceptor->socket, &listenaddr);
			acceptor->channel = channel_create(loop, acceptor->socket.sockfd);
			if(TEST_VAILD_PTR(acceptor->channel)) {
				channel_event_entry entry;
				entry.args = acceptor;
				entry.callback = inner_read_handle;
				channel_set_readentry(acceptor->channel, entry);

				return acceptor;
				channel_destroy(&acceptor->channel);
			}

			socket_close(&acceptor->socket);
			close(acceptor->idlefd);
		}

		FREE(acceptor);
	}

	return acceptor;
}

void acceptor_destroy(net_acceptor **acceptor) {
	if(TEST_VAILD_PTR(acceptor) &&
			TEST_VAILD_PTR(*acceptor)) {
		channel_disable_all((*acceptor)->channel);
		channel_remove((*acceptor)->channel);
		channel_destroy(&(*acceptor)->channel);
		socket_close(&(*acceptor)->socket);
		close((*acceptor)->idlefd);
		FREE(*acceptor);
	}
}

void acceptor_listen(net_acceptor *acceptor) {
	CHECK_VAILD_PTR(acceptor);
	eventloop_check_inloopthread(acceptor->loop);
	atomic_set(&acceptor->listenning, true);
	socket_listen(&acceptor->socket);
	channel_enable_read(acceptor->channel);
}

void acceptor_set_newconnentry(net_acceptor *acceptor, server_newconnection_entry entry) {
	CHECK_VAILD_PTR(acceptor);
	acceptor->entry = entry;
}

net_eventloop *acceptor_get_eventloop(net_acceptor *acceptor) {
	CHECK_VAILD_PTR(acceptor);
	return acceptor->loop;
}

net_socket acceptor_get_sockfd(net_acceptor *acceptor) {
	CHECK_VAILD_PTR(acceptor);
	return acceptor->socket;
}
