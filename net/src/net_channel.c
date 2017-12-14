/*
 * net_channel.c
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifdef __linux__
#define _GNU_SOURCE
#include <sys/epoll.h>
#endif
#include <poll.h>

#include <define.h>
#include <net_eventloop.h>
#include <net_channel.h>

typedef struct net_channel{
	net_eventloop *loop;
	int fd;
	int events;
	int revents;
	int index;	// used by Poller.
	bool loghup;
	weak_ptr tie;
	bool tied;
	bool eventHandling;
	bool addedToLoop;
	channel_event_entry readEntry;
	channel_event_entry writeEntry;
	channel_event_entry closeEntry;
	channel_event_entry errorEntry;
} net_channel;

const int NONE_EVENT = 0;
const int READ_EVENT = POLLIN | POLLPRI;
const int WRITE_EVENT = POLLOUT;

net_channel *channel_create(net_eventloop *loop, int fd) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_FD(fd);
	MALLOC_DEF(channel, net_channel);
	if(TEST_VAILD_PTR(channel)) {
		channel->loop = loop;
		channel->fd = fd;
		channel->events = NONE_EVENT;
		channel->revents = NONE_EVENT;
		channel->index = -1;
		channel->loghup = true;
		channel->tied = false;
		channel->eventHandling = false;
		channel->addedToLoop = false;
		NUL(channel->readEntry.callback);
		NUL(channel->readEntry.args);
		NUL(channel->writeEntry.callback);
		NUL(channel->writeEntry.args);
		NUL(channel->closeEntry.callback);
		NUL(channel->closeEntry.args);
		NUL(channel->errorEntry.callback);
		NUL(channel->errorEntry.args);
	}

	return channel;
}

void channel_destroy(net_channel **channel) {
	if(TEST_VAILD_PTR(channel) &&
			TEST_VAILD_PTR(*channel)) {
		CHECK(!(*channel)->eventHandling);
		CHECK(!(*channel)->addedToLoop);

		if(eventloop_test_inloopthread((*channel)->loop)) {
			CHECK(!eventloop_has_channel((*channel)->loop, *channel));
		}
	}
}

void channel_tie(net_channel *channel, void *obj) {
	CHECK_VAILD_PTR(channel);
	channel->tie.ptr = obj;
	channel->tied = true;
}

void channel_update(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->addedToLoop = true;
	eventloop_update_channel(channel->loop, channel);
}

void channel_remove(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	CHECK(channel_check_noneevent(channel));
	channel->addedToLoop = false;
	eventloop_remove_channel(channel->loop, channel);
}

void channel_handle_event(net_channel *channel, timestamp ts) {
	CHECK_VAILD_PTR(channel);
	// printf("handle event %d\n", channel->revents);
	channel->eventHandling = true;
	if ((channel->revents & POLLHUP) && !(channel->revents & POLLIN)) {
		if (channel->loghup) {
			// LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
		}

		if (channel->closeEntry.callback) {
			printf("socket %d has closed, but I will call close entry for it.", channel_get_fd(channel));
			channel->closeEntry.callback(channel->closeEntry.args, ts);
		} else {
			fprintf(stderr, "socket %d has closed, but I cant't find close entry for it.", channel_get_fd(channel));
		}
	}

	if (channel->revents & POLLNVAL) {
		fprintf(stderr, "POLLNVAL!!!\n");
	}

	if (channel->revents & (POLLERR | POLLNVAL)) {
		fprintf(stderr, "POLLERR | POLLNVAL!!!\n");
		if (channel->errorEntry.callback)
			channel->errorEntry.callback(channel->errorEntry.args, ts);
	}

	if (channel->revents & (POLLIN | POLLPRI /*| POLLRDHUP*/)) {
		if (channel->readEntry.callback)
			channel->readEntry.callback(channel->readEntry.args, ts);
	}

	if (channel->revents & POLLOUT) {
		if (channel->writeEntry.callback)
			channel->writeEntry.callback(channel->writeEntry.args, ts);
	}

	channel->eventHandling = false;
}

static inline stringpiece impl_events2strpie(int fd, int ev) {
	stringpiece strpie;
	char buf[32];
	snprintf(buf, sizeof buf, "%d : ", fd);
	stringpiece_init_cstring(&strpie, buf);
	if (ev & POLLIN)
		stringpiece_append(&strpie, "IN ");
	if (ev & POLLPRI)
		stringpiece_append(&strpie, "PRI  ");
	if (ev & POLLOUT)
		stringpiece_append(&strpie, "OUT ");
	if (ev & POLLHUP)
		stringpiece_append(&strpie, "HUP  ");
	/*
	if (ev & POLLRDHUP)
		stringpiece_append("RDHUP  ");
	*/
	if (ev & POLLERR)
		stringpiece_append(&strpie, "ERR  ");
	if (ev & POLLNVAL)
		stringpiece_append(&strpie, "NVAL  ");

	return strpie;
}

stringpiece channel_events2strpie(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return impl_events2strpie(channel->fd, channel->events);
}

stringpiece channel_revents2strpie(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return impl_events2strpie(channel->fd, channel->revents);
}

int channel_get_fd(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->fd;
}

bool channel_check_noneevent(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->events == NONE_EVENT;
}

void channel_set_revents(net_channel *channel, int revents) {
	CHECK_VAILD_PTR(channel);
	channel->revents = revents;
}

int channel_get_events(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->events;
}

int channel_get_index(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->index;
}

void channel_set_index(net_channel *channel, int index) {
	CHECK_VAILD_PTR(channel);
	channel->index = index;
}

void channel_set_readentry(net_channel *channel, channel_event_entry entry) {
	CHECK_VAILD_PTR(channel);
	CHECK_VAILD_PTR(entry.callback);
	channel->readEntry = entry;
}

void channel_set_writeentry(net_channel *channel, channel_event_entry entry) {
	CHECK_VAILD_PTR(channel);
	CHECK_VAILD_PTR(entry.callback);
	channel->writeEntry = entry;
}

void channel_set_closeentry(net_channel *channel, channel_event_entry entry) {
	CHECK_VAILD_PTR(channel);
	CHECK_VAILD_PTR(entry.callback);
	channel->closeEntry = entry;
}

void channel_set_errorentry(net_channel *channel, channel_event_entry entry) {
	CHECK_VAILD_PTR(channel);
	CHECK_VAILD_PTR(entry.callback);
	channel->errorEntry = entry;
}

void channel_enable_read(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->events |= READ_EVENT;
	channel_update(channel);
}

void channel_disable_read(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->events &= ~READ_EVENT;
	channel_update(channel);
}

void channel_enable_write(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->events |= WRITE_EVENT;
	channel_update(channel);
}

void channel_disable_write(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->events &= ~WRITE_EVENT;
	channel_update(channel);
}

void channel_disable_all(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->events = NONE_EVENT;
	channel_update(channel);
}

bool channel_can_read(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->events & READ_EVENT;
}

bool channel_can_write(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->events & WRITE_EVENT;
}

void channel_close_loghup(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	channel->loghup = false;
}

net_eventloop *channel_get_ownerloop(net_channel *channel) {
	CHECK_VAILD_PTR(channel);
	return channel->loop;
}
