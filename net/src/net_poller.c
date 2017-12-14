/*
 * net_poller.c
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#include <poll.h>
#include <errno.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

#include <array.h>
#include <net_channel.h>
#include <net_eventloop.h>
#include <net_poller.h>

FORWARD_DECLAR(net_eventloop)

#define INIT_POLL_SIZE		1024
#define MAX_CHANNEL_SIZE		65536

typedef struct net_poller {
	net_channel *channels[MAX_CHANNEL_SIZE];
	int channelNum;
	net_eventloop *loop;
#ifdef __linux__
	int epollfd;
	ARRAY events;	// struct epoll_event
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
	ARRAY pollfds;	// struct pollfd
#endif
} net_poller;

net_poller *poller_create(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	MALLOC_DEF(poller, net_poller);
	if(TEST_VAILD_PTR(poller)) {
		poller->loop = loop;
#ifdef __linux__
		poller->epollfd = epoll_create1(EPOLL_CLOEXEC);
		ARRAY_NEW(poller->events);
		if(TEST_VAILD_FD(poller->epollfd) &&
				TEST_VAILD_PTR(poller->events)) {
			ARRAY_RESIZE(poller->events, struct epoll_event, INIT_POLL_SIZE);
			ARRAY_CLEAR(poller->pollfds);
		} else {
			FREE(poller);
		}
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
		ARRAY_NEW(poller->pollfds);
		ARRAY_RESIZE(poller->pollfds, struct pollfd, INIT_POLL_SIZE);
		ARRAY_CLEAR(poller->pollfds);
		if(!TEST_VAILD_PTR(poller->pollfds)) {
			FREE(poller);
		}
#endif
		ZERO(poller->channels);
	}

	return poller;
}

void poller_destroy(net_poller **poller) {
	if(TEST_VAILD_PTR(poller) &&
			TEST_VAILD_PTR(*poller)) {
#ifdef __linux__
		close(poller->epollfd);
		ARRAY_DESTROY(poller->events);
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
		ARRAY_DESTROY((*poller)->pollfds);
#endif
		FREE(*poller);
	}
}

void poller_check_inloopthread(net_poller *poller) {
	CHECK_VAILD_PTR(poller);
	eventloop_check_inloopthread(poller->loop);
}

#ifdef __linux__
void poller_fill_activechannels(net_poller *poller, int num, ARRAY channels) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channels);
	CHECK(num <= ARRAY_SIZE(poller->events, struct epoll_event));
	for(int i = 0; i < num; ++ i) {
		struct epoll_event *event = ARRAY_AT_PTR(poller->events, struct epoll_event, i);
		net_channel* channel = event->data.ptr;
#ifndef NDEBUG
		int fd = channel_get_fd(channel);
		CHECK_VAILD_PTR(poller->channels[fd]);
#endif	/* NDEBUG */
		channel_set_revents(channel, event->events);
		ARRAY_PUSH_BACK(channels, net_channel *, channel);
	}
}

static inline void
impl_poller_update(net_poller *poller, int operation, net_channel* channel) {
	struct epoll_event event;
	bzero(&event, sizeof event);
	event.events = channel_get_events(channel);
	event.data.ptr = channel;
	int fd = channel_get_fd(channel);
	if (epoll_ctl(poller->epollfd, operation, fd, &event) < 0) {
		if (operation == EPOLL_CTL_DEL) {
			// LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
		} else {
			// LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
		}
	}
}

const int NEW_CHANNEL = -1;
const int OLD_CHANNEL = 1;
const int DEL_CHANNEL = 2;

void poller_update_channel(net_poller *poller, net_channel *channel) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channel);

	poller_check_inloopthread(poller);
	const int index = channel_get_index();
	if (index == NEW_CHANNEL || index == DEL_CHANNEL) {
	    // a new one, add with EPOLL_CTL_ADD
	    int fd = channel->fd();
	    if (index == NEW_CHANNEL) {
	    		CHECK(!poller->channels[fd]);
	    		poller->channels[fd] = channel;
	    } else {
	    		// index == kDeleted
	    		CHECK(poller->channels[fd] == channel);
	    }

	    channel_set_index(channel, OLD_CHANNEL);
	    impl_poller_update(poller, EPOLL_CTL_ADD, channel);
	} else {
	    // update existing one with EPOLL_CTL_MOD/DEL
	    int fd = channel_get_fd(channel);
	    (void)fd;
	    CHECK(poller->channels[fd] == channel);
	    CHECK(index == OLD_CHANNEL);
	    if (channel_check_isnoneevent(channel)) {
	    		impl_poller_update(poller, EPOLL_CTL_DEL, channel);
	    		channel_set_index(DEL_CHANNEL);
	    } else {
	    		impl_poller_update(poller, EPOLL_CTL_MOD, channel);
	    }
	}
}

void poller_remove_channel(net_poller *poller, net_channel* channel) {
	poller_check_inloopthread(poller);
	int fd = channel_get_fd(channel);
	CHECK_VAILD_PTR(poller->channels[fd]);
	CHECK(channel_check_noneevent(channel));
	int index = channel_get_index();
	CHECK(index == OLD_CHANNEL || index == DEL_CHANNEL);
	poller->channels[fd] = NULL;
	if (index == OLD_CHANNEL) {
		impl_poller_update(poller, EPOLL_CTL_DEL, channel);
	}

	channel_set_index(channel, NEW_CHANNEL);
}

timestamp poller_poll(net_poller *poller, int ms, ARRAY* channels) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channels);
	int size = ARRAY_SIZE(poller->events, struct epoll_event);
	int num = epoll_wait(ARRAY_BEGIN(poller->epollfd, struct epoll_event),
			poller->events, size, ms);
	  int savedErrno = errno;
	  if (num > 0) {
		  poller_fill_activechannels(poller, num, channels);
		  if ((size_t)num == size) {
			  ARRAY_RESIZE(poller->events, struct epoll_event, size * 2);
		  }
	  } else if (num == 0) {
		  // LOG_TRACE << "nothing happended";
	  } else {
		  // error happens, log uncommon ones
		  if (savedErrno != EINTR) {
			  errno = savedErrno;
			  // LOG_SYSERR << "EPollPoller::poll()";
		  }
	  }

	  return timestamp_now();
}
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
void poller_fill_activechannels(net_poller *poller, int num, ARRAY channels/* net_channel*[] */) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channels);
	CHECK(num <= ARRAY_CAPACITY(poller->pollfds, struct pollfd));
	ARRAY_FOREACH(pfd, poller->pollfds, struct pollfd) {
		if(num <= 0) {
			break;
		}

		if(pfd->revents > 0) {
			-- num;
			net_channel *channel = poller->channels[pfd->fd];
			CHECK_VAILD_PTR(channel);
			CHECK(channel_get_fd(channel) == pfd->fd);
			channel_set_revents(channel, pfd->revents);
			ARRAY_PUSH_BACK(channels, net_channel *, channel);
		}
	}
}

void poller_update_channel(net_poller *poller, net_channel *channel) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channel);
	poller_check_inloopthread(poller);
	if (channel_get_index(channel) < 0) {
		// a new one, add to pollfds
		CHECK(!poller->channels[channel_get_fd(channel)]);
		CHECK(ARRAY_SIZE(poller->pollfds, struct pollfd) < ARRAY_CAPACITY(poller->pollfds, struct pollfd));
		struct pollfd pfd;
		pfd.fd = channel_get_fd(channel);
		pfd.events = (short)channel_get_events(channel);
		pfd.revents = 0;
		ARRAY_PUSH_BACK(poller->pollfds, struct pollfd, pfd);
		int idx = ARRAY_SIZE(poller->pollfds, struct pollfd) - 1;
		channel_set_index(channel, idx);
		poller->channels[pfd.fd] = channel;
	} else {
		// update existing one
		CHECK(poller->channels[channel_get_fd(channel)] == channel);
		int idx = channel_get_index(channel);
		CHECK(0 <= idx && idx < ARRAY_SIZE(poller->pollfds, struct pollfd));
		struct pollfd* pfd = ARRAY_AT_PTR(poller->pollfds, struct pollfd, idx);
		CHECK(pfd->fd == channel_get_fd(channel) || pfd->fd == -channel_get_fd(channel) - 1);
		pfd->events = (short)channel_get_events(channel);
		pfd->revents = 0;

		if (channel_check_noneevent(channel)) {
			// ignore this pollfd
			pfd->fd = -channel_get_fd(channel) - 1;
		}
	}
}

void poller_remove_channel(net_poller *poller, net_channel* channel) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channel);
	poller_check_inloopthread(poller);
	/* 连接关闭时多个地方均调用了channel_remove, 若已经为NULL说明已经调用过了*/
	if(!TEST_VAILD_PTR(poller->channels[channel_get_fd(channel)]))
		return;
	CHECK(poller->channels[channel_get_fd(channel)] == channel);
	CHECK(channel_check_noneevent(channel));
	int idx = channel_get_index(channel);
	CHECK(0 <= idx && idx < ARRAY_SIZE(poller->pollfds, struct pollfd));
	struct pollfd* pfd = ARRAY_AT_PTR(poller->pollfds, struct pollfd, idx);
	CHECK(pfd->fd == -channel_get_fd(channel) - 1
			&& pfd->events == channel_get_events(channel));
	poller->channels[channel_get_fd(channel)] = NULL;

	if ((size_t)idx != ARRAY_SIZE(poller->pollfds, struct pollfd) - 1) {
		struct pollfd *back = ARRAY_BACK(poller->pollfds, struct pollfd);
		struct pollfd *index = ARRAY_AT_PTR(poller->pollfds, struct pollfd, idx);
		int fdAtEnd = back->fd;
		*index = *back;
		if (fdAtEnd < 0) {
			fdAtEnd = -fdAtEnd - 1;
		}
		channel_set_index(poller->channels[fdAtEnd], idx);
	}

	ARRAY_POP_BACK_PTR(poller->pollfds, struct pollfd);
}

timestamp poller_poll(net_poller *poller, int ms, ARRAY channels) {
	// XXX pollfds_ shouldn't change
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channels);
	size_t size = ARRAY_SIZE(poller->pollfds, struct pollfd);
	struct pollfd *arr = ARRAY_BEGIN(poller->pollfds, struct pollfd);
	CHECK_VAILD_PTR(arr);
	printf("before poll, total listen fd num is = %zu\n", size);
	int num = poll(arr, size, ms);
	int savedErrno = errno;
	if (num > 0) {
		printf("%d event happend. total listen fd num is = %zu\n", num, ARRAY_SIZE(poller->pollfds, struct pollfd));
		// CHECK(num <= size);    FIXBUG:有可能是因为添加或删除了监听句柄而被唤醒
		poller_fill_activechannels(poller, num, channels);
	} else if (num == 0) {
	    // LOG_TRACE << " nothing happended";
	} else {
		if (savedErrno != EINTR) {
			errno = savedErrno;
			fprintf(stderr, "poller has happened error, errno is %d.\n", errno);
			ABORT
			// LOG_SYSERR << "PollPoller::poll()";
		}
	}

	return timestamp_now();
}

#endif

bool poller_has_channel(net_poller *poller, net_channel* channel) {
	CHECK_VAILD_PTR(poller);
	CHECK_VAILD_PTR(channel);
	poller_check_inloopthread(poller);

	return TEST_VAILD_PTR(poller->channels[channel_get_fd(channel)]);
}
