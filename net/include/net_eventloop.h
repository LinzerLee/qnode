/*
 * net_eventloop.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_EVENTLOOP_H__
#define __QNODE_NET_EVENTLOOP_H__

#include <net_timer.h>

typedef void(* pending_fn)(void *);

typedef struct pending_entry{
	pending_fn callback;
	void *args;
} pending_entry;

FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_channel)

net_eventloop *eventloop_create();
void eventloop_destroy(net_eventloop **loop);
net_eventloop *eventloop_currentthread();
void eventLoop_wakeup(net_eventloop *loop);
bool eventloop_test_inloopthread(net_eventloop *loop);
void eventloop_check_inloopthread(net_eventloop *loop);
bool eventloop_test_looping(net_eventloop *loop);
bool eventloop_test_quit(net_eventloop *loop);
bool eventloop_test_handling(net_eventloop *loop);
bool eventloop_test_calling(net_eventloop *loop);
void eventloop_quit(net_eventloop *loop);
void eventloop_pending_func(net_eventloop *loop, pending_entry entry);
void eventloop_run_pending(net_eventloop *loop, pending_entry entry);
net_timerid eventloop_settimer_at(net_eventloop *loop,
		timestamp ts, pending_entry entry);
net_timerid eventloop_settimer_after(net_eventloop *loop,
		double delay, pending_entry entry);
net_timerid eventloop_settimer_every(net_eventloop *loop,
		double interval, pending_entry entry);
bool eventloop_cancel_timer(net_eventloop *loop, net_timerid timerId);
bool eventloop_has_channel(net_eventloop *loop, net_channel* channel);
void eventloop_update_channel(net_eventloop *loop, net_channel* channel);
void eventloop_remove_channel(net_eventloop *loop, net_channel* channel);
void eventloop_do_pendingfunc(net_eventloop *loop);
void eventloop_run_loop(net_eventloop *loop);
void eventloop_asgin_owner(net_eventloop *loop);

#endif /* __QNODE_NET_EVENTLOOP_H__ */
