/*
 * net_channel.h
 *
 *  Created on: 2017年10月27日
 *      Author: linzer
 */

#ifndef __QNODE_NET_CHANNEL_H__
#define __QNODE_NET_CHANNEL_H__

#include <timestamp.h>
#include <stringpiece.h>

typedef void(*channel_event_callback)(void *, timestamp);

typedef struct event_entry{
	channel_event_callback callback;
	void *args;
} channel_event_entry;

FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(net_channel)

net_channel *channel_create(net_eventloop *loop, int fd);
void channel_destroy(net_channel **channel);
void channel_tie(net_channel *channel, void *obj);
void channel_update(net_channel *channel);
void channel_remove(net_channel *channel);
void channel_handle_event(net_channel *channel, timestamp ts);
stringpiece channel_events2strpie(net_channel *channel);
stringpiece channel_revents2strpie(net_channel *channel);
int channel_get_fd(net_channel *channel);
void channel_set_revents(net_channel *channel, int revents);
bool channel_check_noneevent(net_channel *channel);
int channel_get_events(net_channel *channel);
int channel_get_index(net_channel *channel);
void channel_set_index(net_channel *channel, int index);
void channel_set_readentry(net_channel *channel, channel_event_entry entry);
void channel_set_writeentry(net_channel *channel, channel_event_entry entry);
void channel_set_closeentry(net_channel *channel, channel_event_entry entry);
void channel_set_errorentry(net_channel *channel, channel_event_entry entry);
void channel_enable_read(net_channel *channel);
void channel_disable_read(net_channel *channel);
void channel_enable_write(net_channel *channel);
void channel_disable_write(net_channel *channel);
void channel_disable_all(net_channel *channel);
bool channel_can_read(net_channel *channel);
bool channel_can_write(net_channel *channel);
void channel_close_loghup(net_channel *channel);
net_eventloop *channel_get_ownerloop(net_channel *channel);

#endif /* __QNODE_NET_CHANNEL_H__ */
