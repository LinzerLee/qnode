/*
 * context.h
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#ifndef __QNODE_CONTEXT_H__
#define __QNODE_CONTEXT_H__

#include <define.h>

FORWARD_DECLAR(message)
FORWARD_DECLAR(context)
FORWARD_DECLAR(net_eventloop)
FORWARD_DECLAR(service)

context *global_context();
int context_init();
int context_get_nodeid();
int context_register_eventloop(net_eventloop *loop);
net_eventloop *context_select_eventloop();
void context_release();
int context_register_mailbox(HANDLE handle);
void context_unregister_mailbox(HANDLE handle);
void context_send_mail(HANDLE handle, message *msg);
message *context_recv_mail(HANDLE handle);
message *context_try_recv_mail(HANDLE handle);
bool context_has_mail(HANDLE handle);
#endif /* __QNODE_CONTEXT_H__ */
