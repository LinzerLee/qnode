/*
 * net_timer.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_TIMER_H__
#define __QNODE_NET_TIMER_H__

#include <timestamp.h>

typedef void(* timer_callback)(void *);

typedef struct net_timer_entry{
	timer_callback callback;
	void *args;
} timer_entry;

FORWARD_DECLAR(net_timer)

typedef struct net_timerid {
	net_timer* timer;
	int64_t sequence;
} net_timerid;

FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_timermanager)

net_timermanager *timermanager_create(net_eventloop* loop);
void timermanager_destroy(net_timermanager **manager);
int timermanager_timeout(net_timermanager *manager);
void timermanager_handle_expired(net_timermanager *manager);
net_timerid timermanager_add_timer(net_timermanager *manager, timer_entry entry,
		timestamp when, double interval);
bool timermanager_cancel_timer(net_timermanager *manager, net_timerid id);

#endif /* __QNODE_NET_TIMER_H__ */
