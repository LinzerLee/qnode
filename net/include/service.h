/*
 * service.h
 *
 *  Created on: 2017年11月1日
 *      Author: linzer
 */

#ifndef __QNODE_SERVICE_H__
#define __QNODE_SERVICE_H__

#include <define.h>

#define PORT_POOL_SIZE (1 << (sizeof(uint16_t) << 3))
#define SERVICE_POOL_SIZE (1 << (sizeof(uint16_t) << 3))
#define INVAILD_SERVICE_HANDLE (SERVICE_POOL_SIZE - 1)

#define SERVICE_STAGE_MAX	16

typedef enum {
	SIG_SERVICE_INIT,
	SIG_SERVICE_START,
	SIG_SERVICE_STOP,
	SIG_SERVICE_RELEASE
} signalno;

FORWARD_DECLAR(message)
FORWARD_DECLAR(buffer)

typedef message *(* unpake_fn)(buffer *buf);

typedef enum {
	SERVICE_NOSTART,
	SERVICE_INITING,
	SERVICE_STARTING,
	SERVICE_STARTED,
	SERVICE_STOPPING,
	SERVICE_STOPPED,
} service_state;

typedef enum {
	TYPE_SERVICE,
	TYPE_SERVLET
} service_type;

typedef struct {
	char *name;
	unpake_fn fn;
} service_protocol;

FORWARD_DECLAR(logger)

int service_init();
void service_release();
HANDLE service_register(const char *sname, service_type type);
// void service_register_type(HANDLE service_handle, service_type type);
void service_switch_type(HANDLE service_handle, service_type type);
void service_unregister();
bool service_register_protocol(service_protocol *protocol);
void service_unregister_protocol(const char *name);
unpake_fn service_query_unpack(const char *name);
int service_config_protocol(uint16_t port, const char *name);
int service_register_port(HANDLE service_handle, uint16_t port, const char *proto);
int service_boost(const char *sname);
int service_batch_boost(const char *service_batch[][SERVICE_STAGE_MAX]);
void service_stop(const char *sname, HANDLE service_handle);
HANDLE service_get_handle(const char *sname);
logger *service_get_logger(HANDLE service_handle);
uint32_t service_get_harborid(HANDLE handle);
int service_wait(const char *sname, STAGE_TYPE type, service_state state, double timeout_s);
int service_batch_wait(const char *snames[], STAGE_TYPE type, service_state state, double timeout_s);
void service_push_message(HANDLE service_handle, message *msg);
message *service_pop_message(HANDLE service_handle);
void service_handle_trash();
void service_dispatch_message();
#endif /* __QNODE_SERVICE_H__ */
