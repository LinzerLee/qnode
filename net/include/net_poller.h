/*
 * net_poller.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_POLLER_H__
#define __QNODE_NET_POLLER_H__

#include <timestamp.h>
// #define __linux__
FORWARD_DECLAR(net_poller)
FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_channel)

net_poller *poller_create(net_eventloop *loop);
void poller_destroy(net_poller **poller);
void poller_check_inloopthread(net_poller *poller);
void poller_fill_activechannels(net_poller *poller, int num, ARRAY channels);
void poller_update_channel(net_poller *poller, net_channel *channel);
void poller_remove_channel(net_poller *poller, net_channel* channel);
timestamp poller_poll(net_poller *poller, int ms, ARRAY channels);
bool poller_has_channel(net_poller *poller, net_channel* channel);
#endif /* __QNODE_INCLUDE_NET_POLLER_H__ */
