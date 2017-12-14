/*
 * message.h
 *
 *  Created on: 2017年11月1日
 *      Author: linzer
 */

#ifndef __QNODE_MESSAGE_H__
#define __QNODE_MESSAGE_H__
#include <stdint.h>

#include <queue.h>
typedef enum {
	MSG_RAW = 1,
	MSG_REQ = 2,
	MSG_REP = 4,
	MSG_CPY = 8,
	MSG_SHA = 16
} message_type;

typedef struct message {
	int type;
	uint32_t source;		/* 对于raw类型的消息source表示ip地址，而rpc类型的消息source表示service_id = node_id << 16 & service_handle */
	int session;			/* 对于raw类型的消息session永远为0 */
	void *data;
	size_t size;
	queue_node node;
	int errcode;			/* RPC类型有效 */
	int sockfd;			/* 注册端口的有效 */
} message;

#define MSG_IS_RAW(msg)		(!!((msg->type) & MSG_RAW))
#define MSG_IS_REQ(msg)		(!!((msg->type) & MSG_REQ))
#define MSG_IS_REP(msg)		(!!((msg->type) & MSG_REP))
#define MSG_IS_CPY(msg)		(!!((msg->type) & MSG_CPY))
#define MSG_IS_SHA(msg)		(!!((msg->type) & MSG_SHA))

typedef void(* call_success_cb)(message *msg);
typedef void(* call_faild_cb)(message *msg);

message *quick_gen_msg(HANDLE self, int session, void *data, size_t size, int type);
int qsend(const char*sname, void *data, size_t size, bool share);
int qcall(const char*sname, void *data, size_t size,
		call_success_cb scb, call_faild_cb fcb);
int qreturn(HANDLE peer, int err, void *data,
		size_t size, int session);
#endif /* __QNODE_MESSAGE_H__ */
