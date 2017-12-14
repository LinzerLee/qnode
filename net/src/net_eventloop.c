/*
 * net_eventloop.c
 *
 *  Created on: 2017年10月28日
 *      Author: linzer
 */

#include <signal.h>
#ifdef __linux__
#include <sys/eventfd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#include <unistd.h>
#endif

#include <define.h>
#include <atomic.h>
#include <thread.h>
#include <mutex.h>
#include <array.h>
#include <timestamp.h>
#include <net_socket.h>
#include <net_channel.h>
#include <net_poller.h>
#include <net_eventloop.h>

typedef struct net_eventloop {
	atomic_t looping; 				/* bool atomic */
	atomic_t quit; 					/* bool atomic */
	atomic_t handling; 				/* bool atomic */
	atomic_t calling; 				/* bool atomic */
	int64_t iteration;
	timestamp pollReturn;
	net_poller *poller;
	net_timermanager *timermgr;
	int wakeupfd[2];
	net_channel *wakeupChannel;
	ARRAY activeChannels;			/* net_channel * */
	net_channel *currentChannel;
	ARRAY pendingFuncs;				/* pending_entry */
	int owner;
	mutex lock;
} net_eventloop;

__thread net_eventloop *t_loopInThisThread = NULL;

static inline void wakeup_read_handle(void *data, timestamp ts) {
	net_eventloop *loop = (net_eventloop *)data;
	CHECK_VAILD_PTR(loop);
	uint64_t one = 1;
	ssize_t n = read(loop->wakeupfd[0], &one, sizeof one);
	if (n != sizeof one) {
		// LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
	}
}

void eventloop_asgin_owner(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	loop->owner = thread_current_id();
}

net_eventloop *eventloop_create() {
	if(TEST_VAILD_PTR(t_loopInThisThread)) {
		printf("eventloop has exist!\n");
		return NULL;
	}

	signal(SIGPIPE, SIG_IGN);

	MALLOC_DEF(loop, net_eventloop);
	if(TEST_VAILD_PTR(loop)) {
		atomic_set(&loop->looping, false);
		atomic_set(&loop->quit, false);
		atomic_set(&loop->handling, false);
		atomic_set(&loop->calling, false);
		loop->iteration = 0;
		NUL(loop->currentChannel);
		loop->pollReturn = timestamp_invaild();
		eventloop_asgin_owner(loop);
		MUTEX_INIT(loop);
		int ret = INVAILD_FD;
#ifdef __linux__
		loop->wakeupfd[0] = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		loop->wakeupfd[1] = loop->wakeupfd[0];
		ret = loop->wakeupfd[0];
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
		ret = pipe(loop->wakeupfd);
#endif
		if(ret >= 0) {
			loop->wakeupChannel = channel_create(loop, loop->wakeupfd[0]);
			if(TEST_VAILD_PTR(loop->wakeupChannel)) {
				loop->poller = poller_create(loop);
				if(TEST_VAILD_PTR(loop->poller)) {
					loop->timermgr = timermanager_create(loop);
					if(TEST_VAILD_PTR(loop->timermgr)) {
						ARRAY_NEW(loop->activeChannels);
						if(TEST_VAILD_PTR(loop->activeChannels)) {
							ARRAY_NEW(loop->pendingFuncs);
							if(TEST_VAILD_PTR(loop->pendingFuncs)) {
								t_loopInThisThread = loop;
								channel_event_entry entry;
								entry.callback = wakeup_read_handle;
								entry.args = loop;
								channel_set_readentry(loop->wakeupChannel, entry);
								channel_enable_read(loop->wakeupChannel);
								return loop;
							}

							ARRAY_DESTROY(loop->activeChannels);
						}

						timermanager_destroy(&loop->timermgr);
					}

					poller_destroy(&loop->poller);
				}

				channel_destroy(&loop->wakeupChannel);
			}

			if(TEST_VAILD_FD(loop->wakeupfd[0]))
				close(loop->wakeupfd[0]);
			if(TEST_VAILD_FD(loop->wakeupfd[1]))
				close(loop->wakeupfd[1]);
		}

		FREE(loop);
	}

	return loop;
}

void eventloop_destroy(net_eventloop **loop) {
	if(TEST_VAILD_PTR(loop) && TEST_VAILD_PTR(*loop)) {
		// LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_ << " destructs in thread " << CurrentThread::tid();
		channel_disable_all((*loop)->wakeupChannel);
		channel_remove((*loop)->wakeupChannel);
		close((*loop)->wakeupfd[0]);

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
		close((*loop)->wakeupfd[1]);
#endif
		NUL(t_loopInThisThread);

		channel_destroy(&(*loop)->wakeupChannel);
		poller_destroy(&(*loop)->poller);
		timermanager_destroy(&(*loop)->timermgr);
		ARRAY_DESTROY((*loop)->activeChannels);
		ARRAY_DESTROY((*loop)->pendingFuncs);
		MUTEX_DESTROY(*loop);
		FREE(*loop);
	}
}

net_eventloop *eventloop_currentthread() {
	return t_loopInThisThread;
}

void eventLoop_wakeup(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	uint64_t one = 1;
	ssize_t n = write(loop->wakeupfd[1], &one, sizeof one);
	if (n != sizeof one) {
		// LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
	}
}

bool eventloop_test_inloopthread(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	// printf("loop->owner = %d, thread_current_id = %d\n", loop->owner, thread_current_id());
	return loop->owner == thread_current_id();
}

void eventloop_check_inloopthread(net_eventloop *loop) {
	CHECK(eventloop_test_inloopthread(loop));
}

bool eventloop_test_looping(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	return atomic_get(&loop->looping);
}

bool eventloop_test_quit(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	return atomic_get(&loop->quit);
}

bool eventloop_test_handling(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	return atomic_get(&loop->handling);
}

bool eventloop_test_calling(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	return atomic_get(&loop->calling);
}

void eventloop_quit(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	atomic_set(&loop->quit, true);
	if (!eventloop_test_inloopthread(loop)) {
		eventLoop_wakeup(loop);
	}
}

void eventloop_pending_func(net_eventloop *loop, pending_entry entry) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(entry.callback);
	MUTEX_LOCK(loop);
	ARRAY_PUSH_BACK(loop->pendingFuncs, pending_entry, entry);
	MUTEX_UNLOCK(loop);
	if(!eventloop_test_inloopthread(loop) && !eventloop_test_calling(loop)) {
		eventLoop_wakeup(loop);
	}
}

void eventloop_run_pending(net_eventloop *loop, pending_entry entry) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(entry.callback);
	if(eventloop_test_inloopthread(loop)) {
		entry.callback(entry.args);
	} else {
		eventloop_pending_func(loop, entry);
	}
}

net_timerid eventloop_settimer_at(net_eventloop *loop,
		timestamp ts, pending_entry entry) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(entry.callback);
	timer_entry te;
	te.callback = entry.callback;
	te.args = entry.args;

	return timermanager_add_timer(loop->timermgr, te, ts, 0.0);
}

net_timerid eventloop_settimer_after(net_eventloop *loop,
		double delay, pending_entry entry) {
	timestamp ts = timestamp_delay(timestamp_now(), delay);

	return eventloop_settimer_at(loop, ts, entry);
}

net_timerid eventloop_settimer_every(net_eventloop *loop,
		double interval, pending_entry entry) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(entry.callback);
	timer_entry te;
	te.callback = entry.callback;
	te.args = entry.args;
	timestamp ts = timestamp_delay(timestamp_now(), interval);

	return timermanager_add_timer(loop->timermgr, te, ts, interval);
}

bool eventloop_cancel_timer(net_eventloop *loop, net_timerid timerId) {
	CHECK_VAILD_PTR(loop);
	return timermanager_cancel_timer(loop->timermgr, timerId);
}

bool eventloop_has_channel(net_eventloop *loop, net_channel* channel) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(channel);
	CHECK(channel_get_ownerloop(channel) == loop);
	eventloop_check_inloopthread(loop);

	return poller_has_channel(loop->poller, channel);
}

void eventloop_update_channel(net_eventloop *loop, net_channel* channel) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(channel);
	CHECK(channel_get_ownerloop(channel) == loop);
	eventloop_check_inloopthread(loop);

	poller_update_channel(loop->poller, channel);
}

void eventloop_remove_channel(net_eventloop *loop, net_channel* channel) {
	CHECK_VAILD_PTR(loop);
	CHECK_VAILD_PTR(channel);
	CHECK(channel_get_ownerloop(channel) == loop);
	eventloop_check_inloopthread(loop);

	if (atomic_get(&loop->handling)) {
		if(loop->currentChannel != channel) {
			bool is_active = true;
			ARRAY_FOREACH(ptr, loop->activeChannels, net_channel *) {
				if(*ptr == channel) {
					is_active = false;
					break;
				}
			}
			CHECK(is_active);
		}
	}

	poller_remove_channel(loop->poller, channel);
}

void eventloop_do_pendingfunc(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	ARRAY_CREATE(newpending);
	ATOMIC_TRUE(loop->calling);
	MUTEX_LOCK(loop);
	ARRAY tmp = loop->pendingFuncs;
	loop->pendingFuncs = newpending;
	newpending = tmp;
	MUTEX_UNLOCK(loop);

	ARRAY_FOREACH(entry, tmp, pending_entry) {
		entry->callback(entry->args);
	}

	ARRAY_DESTROY(tmp);
	ATOMIC_FALSE(loop->calling);
}

static const int DEFAULT_POLL_TIMEOUT_MS = -1;
static const int MAX_SAFE_POLL_TIMEOUT_MS = 1789569;

void inner_run_loop(net_eventloop *loop, int timeout) {
	ARRAY_CLEAR(loop->activeChannels);
	loop->pollReturn = poller_poll(loop->poller, timeout, loop->activeChannels);
	++ loop->iteration;
	atomic_set(&loop->handling, true);
	ARRAY_FOREACH(ptr, loop->activeChannels, net_channel *) {
		loop->currentChannel = *ptr;
		channel_handle_event(loop->currentChannel, loop->pollReturn);
	}

	NUL(loop->currentChannel);
	atomic_set(&loop->handling, false);
	eventloop_do_pendingfunc(loop);
}

void eventloop_run_loop(net_eventloop *loop) {
	CHECK_VAILD_PTR(loop);
	CHECK(!atomic_get(&loop->looping));
	eventloop_check_inloopthread(loop);
	atomic_set(&loop->quit, false);

	int timeout, real_timeout;
	timestamp basetime;
	while (!atomic_get(&loop->quit)) {
		basetime = timestamp_now();
		timeout = timermanager_timeout(loop->timermgr);
		CHECK(timeout >= DEFAULT_POLL_TIMEOUT_MS);
		timeout = timeout > MAX_SAFE_POLL_TIMEOUT_MS ? MAX_SAFE_POLL_TIMEOUT_MS : timeout;
		real_timeout = timeout;

		while(true) {
			atomic_set(&loop->looping, true);
			inner_run_loop(loop, timeout);
			atomic_set(&loop->looping, false);

			int diff = (int)(timestamp_diff(timestamp_now(), basetime) * 1000);
			real_timeout -= diff;
			if (real_timeout <= 0)
			      break;

			timeout = real_timeout;
		}
	}
}
