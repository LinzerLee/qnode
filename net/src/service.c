/*
 * service.c
 *
 *  Created on: 2017年11月1日
 *      Author: linzer
 */
#include <stdint.h>

#include <spinlock.h>
#include <stringpiece.h>
#include <atomic.h>
#include <array.h>
#include <queue.h>
#include <buffer.h>
#include <message.h>
#include <net.h>
#include <context.h>
#include <thread.h>
#include <module.h>
#include <service.h>
#include <logger.h>

typedef struct {
	uint16_t port;
	int sockfd;
	net_acceptor *acceptor;
	ARRAY connections;			/* net_connection **/
	unpake_fn unpack;
} inner_acceptor;

typedef struct service {
	uint16_t handle;			/* INVAILD_SERVICE_HANDLE(uint16_t表示为65535) */
	char *name;
	ARRAY alias;				/* char * */
	ARRAY acceptors;			/* inner_acceptor * */
	service_state state;
	service_type type;
	// block_queue *msg_queue;		/* message queue */
	module *mod;					/* module cache */
	logger *log;					/* logger */
} service;

typedef struct service_pool {
	uint16_t ports[PORT_POOL_SIZE];			/* port-->service handle */
	inner_acceptor *acceptors[PORT_POOL_SIZE]; /* port-->inner_acceptor * */
	service * services[SERVICE_POOL_SIZE]; 	/* service handle-->service  services[INVAILD_SERVICE_HANDLE]不能被使用 */
	ARRAY protocols;			/* service_protocol */
	/* tire *alias */
	spinlock lock;
	// queue *trash_msg;		/* console message queue 使用普通队列不可以等待*/
} service_pool;

static uint16_t g_genServiceHandle = { 0 };
static service_pool *S = NULL;

/* 每次任务调度都需要重新赋值 */
__thread HANDLE t_selfHandle = INVAILD_SERVICE_HANDLE;
__thread service *t_selfService = NULL;

static inline void inner_servicethread_init(HANDLE handle) {
	t_selfHandle = handle;
	t_selfService = S->services[handle];
}

static inline void inner_servicethread_release() {
	t_selfHandle = INVAILD_SERVICE_HANDLE;
	NUL(t_selfService);
}

static inline int inner_service_signal(module *mod, HANDLE handle, signalno no) {
	int ret = ERROR_SUCCESS;
	inner_servicethread_init(handle);
	ret = module_signal(mod, handle, no);
	inner_servicethread_release();

	return ret;
}

static message *default_raw_unpack(buffer *buf) {
	CHECK_VAILD_PTR(buf);
	CHECK(buffer_get_readable(buf) != 0);
	/* size为0且data不为NULL，表示data存放的是buffer指针 */
	message *msg = quick_gen_msg(INVAILD_SERVICE_HANDLE, 0, (void *)buffer_steal(buf), 0, MSG_RAW | MSG_CPY);
	CHECK_VAILD_PTR(msg);

	return msg;
}

static message *default_rpc_unpack(buffer *buf) {
	return NULL;
}

static message *default_http_unpack(buffer *buf) {
	return NULL;
}

static message *default_cmd_unpack(buffer *buf) {
	char *eof = buffer_find_eol(buf);
	if(TEST_VAILD_PTR(eof)) {
		size_t size = eof - buffer_peek(buf) + 1;
		char *data = malloc(size);
		if(TEST_VAILD_PTR(data)) {
			memcpy(data, buffer_peek(buf), size);
			data[size - 1] = '\0';
			message *msg = quick_gen_msg(INVAILD_SERVICE_HANDLE, 0, (void *)data, size, MSG_RAW | MSG_CPY);
			if(TEST_VAILD_PTR(msg)) {
				buffer_retrieve(buf, size);
			}

			return msg;
		}
	} else {
		fprintf(stderr, "%s", "cmd form error!\n");
		buffer_retrieve_all(buf);
	}
	return NULL;
}

static unpake_fn g_defaultProto = default_raw_unpack;

int service_init() {
	MALLOC_DEF(sp, service_pool);
	int errcode = ERROR_FAILD;
	if(TEST_VAILD_PTR(sp)) {
		ARRAY_NEW(sp->protocols);
		if(TEST_VAILD_PTR(sp->protocols)) {
			ZERO(sp->services);
			ZERO(sp->acceptors);
			memset(sp->ports, 0xFF, sizeof sp->ports);
			SPIN_INIT(sp);
			S = sp;

			/* register trash mailbox */
			if(TEST_SUCCESS(context_register_mailbox(INVAILD_SERVICE_HANDLE))) {
				/* S赋值成功后才可以注册协议 */
				service_protocol prot;
				prot.name = strdup("raw");
				prot.fn = default_raw_unpack;
				(void)service_register_protocol(&prot);

				prot.name = strdup("rpc");
				prot.fn = default_rpc_unpack;
				(void)service_register_protocol(&prot);

				prot.name = strdup("http");
				prot.fn = default_http_unpack;
				(void)service_register_protocol(&prot);

				prot.name = strdup("cmd");
				prot.fn = default_cmd_unpack;
				(void)service_register_protocol(&prot);

				errcode = ERROR_SUCCESS;

				return errcode;
			}

			ARRAY_DESTROY(sp->protocols);
		}

		FREE(sp);
	}

	return errcode;
}

void service_release() {
	if(TEST_VAILD_PTR(S)) {
		/* 各种资源的释放 */
		// TODO
		SPIN_LOCK(S);
		context_unregister_mailbox(INVAILD_SERVICE_HANDLE);
		for(int i=0; i<SERVICE_POOL_SIZE-1; ++i) {
			if(TEST_VAILD_PTR(S->services[i])) {
				CHECK(i == S->services[i]->handle);
				service_unregister(i);
				context_unregister_mailbox(i);
			}

		}
		ARRAY_DESTROY(S->protocols);
		SPIN_UNLOCK(S);
		SPIN_DESTROY(S);
		FREE(S);
	}
}

module *service_get_module(HANDLE handle) {
	CHECK_VAILD_SERVICE_HANDLE(handle);
	SPIN_LOCK(S);
	service *s = S->services[handle];
	CHECK_VAILD_PTR(s);
	module *mod = s->mod;
	if(!TEST_VAILD_PTR(mod)) {
		mod = module_query(s->name);
		s->mod = mod;
	}
	SPIN_UNLOCK(S);

	return mod;
}
/*
void service_register_type(HANDLE service_handle, service_type type) {
	CHECK(type >= TYPE_SERVICE && type <= TYPE_SERVLET);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	CHECK_VAILD_PTR(s);
	s->type = type;
	SPIN_UNLOCK(S);
}
*/
void service_switch_type(HANDLE service_handle, service_type type) {
	CHECK(type >= TYPE_SERVICE && type <= TYPE_SERVLET);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	CHECK_VAILD_PTR(s);
	service_type otype = s->type;
	SPIN_UNLOCK(S);

	if(otype != type) {
		if(TYPE_SERVLET == otype) {
			/* servlet->service */
			SPIN_LOCK(S);
			service *s = S->services[service_handle];
			CHECK_VAILD_PTR(s);
			s->type = type;
			SPIN_UNLOCK(S);
		} else if(TYPE_SERVICE == otype){
			service_stop(s->name, INVAILD_SERVICE_HANDLE);
			service_wait(s->name, STAGE_EQUAL, SERVICE_STOPPED, -1);
			SPIN_LOCK(S);
			s->type = type;
			SPIN_UNLOCK(S);
		}
	}
}

HANDLE service_register(const char *sname, service_type type) {
	CHECK_VAILD_PTR(S);
	CHECK(type >= TYPE_SERVICE && type <= TYPE_SERVLET);
	HANDLE handle = INVAILD_SERVICE_HANDLE;
	MALLOC_DEF(s, service);
	if(TEST_VAILD_PTR(s)) {
		s->type = type;
		s->name = strdup(sname);
		NUL(s->mod);
		ARRAY_NEW(s->alias);
		ARRAY_NEW(s->acceptors);
		s->log = logger_create();
		if(TEST_VAILD_PTR(s->name) && TEST_VAILD_PTR(s->alias) &&
				TEST_VAILD_PTR(s->acceptors) && TEST_VAILD_PTR(s->log)) {
			/* services[INVAILD_SERVICE_HANDLE]不能被使用 */
			int tmp = ATOM_INC_OLD(&g_genServiceHandle);
			while(tmp==INVAILD_SERVICE_HANDLE || TEST_VAILD_PTR(S->services[tmp])) {
				tmp = ATOM_INC_OLD(&g_genServiceHandle);
			}

			s->handle = tmp;
			s->state = SERVICE_NOSTART;
			SPIN_LOCK(S);
			S->services[s->handle] = s;
			SPIN_UNLOCK(S);
			/* register mailbox */
			CHECK_SUCCESS(context_register_mailbox(s->handle));
			return handle = s->handle;
		}
		ARRAY_DESTROY(s->alias);
		ARRAY_DESTROY(s->acceptors);
		logger_destroy(&s->log);
		FREE(s);
	}

	return handle;
}

static void inner_connect_destroyed_adapter(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	connection_connect_destroyed(conn);
	/* 释放内存 */
	connection_destroy(&conn);
}

void service_unregister(HANDLE service_handle) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_SERVICE_HANDLE(service_handle);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	CHECK(s->handle == service_handle);
	NUL(S->services[service_handle]);

	SPIN_UNLOCK(S);

	/* release acceptor */
	net_acceptor *acceptor;
	uint16_t port;
	ARRAY_FOREACH(ptr, s->acceptors, inner_acceptor *) {
		port = (*ptr)->port;
		SPIN_LOCK(S);
		acceptor = (*ptr)->acceptor;
		CHECK((*ptr)==S->acceptors[port]);
		NUL(S->acceptors[port]);
		S->ports[port] = INVAILD_SERVICE_HANDLE;
		SPIN_UNLOCK(S);
		/* release connection */
		ARRAY_FOREACH(conn, (*ptr)->connections, net_connection *) {
			pending_entry entry;
			entry.args = (*conn);
			entry.callback = inner_connect_destroyed_adapter;
			/* 回调函数中释放connection内存 */
			eventloop_run_pending(acceptor_get_eventloop(acceptor), entry);
		}
	}

	ARRAY_DESTROY(s->alias);
	ARRAY_DESTROY(s->acceptors);
	/* unregister mailbox */
	context_unregister_mailbox(service_handle);
	logger_destroy(&s->log);
	FREE(s);
}

int service_register_protocol(service_protocol *protocol) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(protocol);
	SPIN_LOCK(S);
	ARRAY_PUSH_BACK(S->protocols, service_protocol, *protocol);
	SPIN_UNLOCK(S);

	return ERROR_SUCCESS;
}

int service_update_protocol(service_protocol *protocol) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(protocol);
	int errcode = ERROR_FAILD;
	SPIN_LOCK(S);
	ARRAY_FOREACH(ptr, S->protocols, service_protocol) {
		if(0 == strcmp(ptr->name, protocol->name)) {
			ptr->fn = protocol->fn;
			errcode = ERROR_SUCCESS;
			break;
		}
	}
	SPIN_UNLOCK(S);
	return ERROR_SUCCESS;
}

void service_unregister_protocol(const char *name) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(name);
	SPIN_LOCK(S);
	ARRAY_FOREACH(ptr, S->protocols, service_protocol) {
		if(0 == strcmp(ptr->name, name)) {
			FREE(ptr->name);
			ARRAY_REMOVE(S->protocols, service_protocol, ARRAY_INDEX(ptr, S->protocols, service_protocol));
			break;
		}
	}
	SPIN_UNLOCK(S);
}

/* name == NULL 返回默认协议 */
unpake_fn service_query_unpack(const char *name) {
	CHECK_VAILD_PTR(S);
	unpake_fn unpack = g_defaultProto;

	if(!TEST_VAILD_PTR(name))
		return unpack;

	SPIN_LOCK(S);
	ARRAY_FOREACH(prot, S->protocols, service_protocol) {
		if(0 == strcmp(name, prot->name)) {
			unpack = prot->fn;
			break;
		}
	}
	SPIN_UNLOCK(S);

	return unpack;
}

int service_config_protocol(uint16_t port, const char *name) {
	CHECK_VAILD_PTR(S);
	SPIN_LOCK(S);
	CHECK_VAILD_PTR(S->acceptors[port]);
	S->acceptors[port]->unpack = service_query_unpack(name);
	SPIN_UNLOCK(S);

	return ERROR_SUCCESS;
}

static atomic_t g_genConnSeq = { 0 };

static void inner_connect_established_adapter(void *args) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	connection_connect_established(conn);
}

static void inner_message_callback(void *args, buffer* buf, timestamp ts) {
	net_connection *conn = (net_connection *)args;
	CHECK_VAILD_PTR(conn);
	net_socket sock = connection_get_socket(conn);
	uint16_t port = socket_get_port(&sock);
	SPIN_LOCK(S);
	unpake_fn unpack = S->acceptors[port]->unpack;
	HANDLE handle = S->ports[port];
	CHECK(handle != INVAILD_SERVICE_HANDLE);
	SPIN_UNLOCK(S);

	message *msg = unpack(buf);
	if(TEST_VAILD_SERVICE_HANDLE(SERVICE_ID(msg->source))) {
		msg->source = socket_get_peeraddr(&sock).addr.sin_addr.s_addr;
	}
	msg->sockfd = sock.sockfd;
	if(TEST_VAILD_PTR(msg)) {
		if(msg->type & MSG_RAW) {
			net_address peeraddr = connection_get_peeraddr(conn);
			msg->source = netendian_ip(&peeraddr);
		}
	}
	msg->source = HARBOR_ID(context_get_nodeid(), handle);
	service_push_message(handle, msg);
}

static void inner_newconn_callback(void *args, net_socket sock, net_address peeraddr) {
	inner_acceptor *inacc = (inner_acceptor *)args;
	CHECK_VAILD_PTR(inacc);
	stringpiece strpie = netaddr2strpie(&peeraddr, false);
	char buf[64];
	snprintf(buf, sizeof buf, "-%s#%d", stringpiece_to_cstring(&strpie), atomic_inc(&g_genConnSeq));
	stringpiece_clear(&strpie);
	// 不能使用t_service，该函数的执行在loop线程中
	SPIN_LOCK(S);
	service *s = S->services[S->ports[inacc->port]];
	CHECK_VAILD_PTR(s);
	stringpiece_append(&strpie, s->name);
	SPIN_UNLOCK(S);
	stringpiece_append(&strpie, buf);

	net_address localaddr = socket_get_localaddr(&sock);
	net_connection *conn = connection_create(acceptor_get_eventloop(inacc->acceptor),
			stringpiece_to_cstring(&strpie), &sock, &localaddr, &peeraddr);

	/* 配置connection各种回调函数 */
	connection_event_entry event_entry;
	event_entry.args = conn;
	event_entry.message_cb = inner_message_callback;
	connection_set_message_entry(conn, event_entry);

	pending_entry entry;
	entry.args = conn;
	entry.callback = inner_connect_established_adapter;
	eventloop_run_pending(acceptor_get_eventloop(inacc->acceptor), entry);
	ARRAY_PUSH_BACK(inacc->connections, net_connection *, conn);
	printf("accept a new connection : sockfd = %d, %s\n", sock.sockfd, stringpiece_to_cstring(&strpie));
	stringpiece_release(&strpie);
}

static void inner_acceptor_listen(void *args) {
	net_acceptor *acceptor = (net_acceptor *)args;
	CHECK_VAILD_PTR(acceptor);
	acceptor_listen(acceptor);
}

int service_register_port(HANDLE service_handle, uint16_t port, const char *proto) {
	context *ctx = global_context();
	CHECK_VAILD_PTR(ctx);
	CHECK_VAILD_SERVICE_HANDLE(service_handle);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	SPIN_UNLOCK(S);
	CHECK_VAILD_PTR(s);
	int errcode = ERROR_FAILD;
	/* 端口没有被占用才能够进行注册 */
	if(INVAILD_SERVICE_HANDLE == S->ports[port]) {
		net_address addr = netaddr4(port, false);
		net_acceptor *acceptor = acceptor_create(context_select_eventloop(), addr, true);

		if(TEST_VAILD_PTR(acceptor)) {
			MALLOC_DEF(inacc, inner_acceptor);
			server_newconnection_entry entry;
			entry.args = inacc;
			entry.callback = inner_newconn_callback;
			acceptor_set_newconnentry(acceptor, entry);
			if(TEST_VAILD_PTR(inacc)) {
				inacc->port = port;
				inacc->acceptor = acceptor;
				inacc->unpack = service_query_unpack(proto);
				inacc->sockfd = acceptor_get_sockfd(acceptor).sockfd;
				ARRAY_NEW(inacc->connections);
				if(TEST_VAILD_PTR(inacc->connections)) {
					SPIN_LOCK(S);
					S->ports[port] = service_handle;
					S->acceptors[port] = inacc;
					ARRAY_PUSH_BACK(s->acceptors, inner_acceptor *, inacc);
					SPIN_UNLOCK(S);
					pending_entry entry;
					entry.args = acceptor;
					entry.callback = inner_acceptor_listen;
					eventloop_run_pending(acceptor_get_eventloop(acceptor), entry);
					// acceptor_listen(acceptor);
					eventLoop_wakeup(acceptor_get_eventloop(acceptor));
					errcode = ERROR_SUCCESS;

					return errcode;
				}

				FREE(inacc);
			}

			acceptor_destroy(&acceptor);
		}
	}

	return errcode;
}

static int inner_service_init(module *mod, HANDLE service_handle) {
	return inner_service_signal(mod, service_handle, SIG_SERVICE_INIT);
}

static int inner_service_start(module *mod, HANDLE service_handle) {
	SPIN_LOCK(S);
	S->services[service_handle]->state = SERVICE_STARTED;
	SPIN_UNLOCK(S);
	service_state state;
	service_type type;
	int errcode = ERROR_SUCCESS;
	do {
		SPIN_LOCK(S);
		state = S->services[service_handle]->state;
		type = S->services[service_handle]->type;
		SPIN_UNLOCK(S);

		if(state != SERVICE_STARTED) {
			break;
		}

		errcode = inner_service_signal(mod, service_handle, SIG_SERVICE_START);
		if(!TEST_SUCCESS(errcode)) {
			break;
		}
	} while(type == TYPE_SERVICE);

	return errcode;
}

static int inner_service_stop(module *mod, HANDLE service_handle) {
	SPIN_LOCK(S);
	S->services[service_handle]->state = SERVICE_STOPPING;
	SPIN_UNLOCK(S);
	return inner_service_signal(mod, service_handle, SIG_SERVICE_STOP);
}

static int inner_service_release(module *mod, HANDLE service_handle) {
	return inner_service_signal(mod, service_handle, SIG_SERVICE_RELEASE);
}

typedef struct {
	const char *sname;
	HANDLE handle;
	module *mod;
} inner_service_boost_data;

static void inner_service_boost(void *args) {
	inner_service_boost_data *data = (inner_service_boost_data *)args;
	CHECK_VAILD_PTR(data);
	const char *sname = data->sname;
	HANDLE handle = data->handle;
	module *mod = data->mod;
	FREE(data);
	printf("%s service starting......\n", sname);
	SPIN_LOCK(S);
	S->services[handle]->state = SERVICE_STARTING;
	SPIN_UNLOCK(S);

	int errcode = inner_service_start(mod, handle);
	if(!TEST_SUCCESS(errcode)) {
		service_unregister(handle);
		fprintf(stderr, "%s service start faild!\n", sname);
		ABORT
	}

	printf("%s service stopping......\n", sname);
	SPIN_LOCK(S);
	S->services[handle]->state = SERVICE_STOPPING;
	SPIN_UNLOCK(S);
	errcode = inner_service_release(mod, handle);
	if(!TEST_SUCCESS(errcode)) {
		service_unregister(handle);
		fprintf(stderr, "%s service release faild!\n", sname);
		ABORT
	}

	printf("%s service stoped......\n", sname);
	SPIN_LOCK(S);
	S->services[handle]->state = SERVICE_STOPPED;
	SPIN_UNLOCK(S);
}

int service_boost(const char *sname) {
	CHECK_VAILD_PTR(sname);
	module *mod = module_query(sname);
	int errcode = ERROR_FAILD;
	if(TEST_VAILD_PTR(mod)) {
		HANDLE handle = service_register(sname, TYPE_SERVLET);
		CHECK(handle != INVAILD_SERVICE_HANDLE);

		printf("%s service initing......\n", sname);
		SPIN_LOCK(S);
		S->services[handle]->state = SERVICE_INITING;
		SPIN_UNLOCK(S);
		errcode = inner_service_init(mod, handle);

		if(!TEST_SUCCESS(errcode)) {
			service_unregister(handle);
			fprintf(stderr, "%s service init faild!\n", sname);
			ABORT
		}

		if(TYPE_SERVICE == S->services[handle]->type) {
			MALLOC_DEF(data, inner_service_boost_data);
			data->sname = sname;
			data->handle = handle;
			data->mod = mod;
			const char *tname = sname;
			threadpool_apply_service(inner_service_boost, (void *)data, tname);
		}
	} else {
		fprintf(stderr, "%s service boost faild!\n", sname);
		ABORT
	}

	return errcode;
}

int service_batch_boost(const char *service_batch[][SERVICE_STAGE_MAX]) {
	int errcode = ERROR_SUCCESS;
	int i = 0;
	while(TEST_VAILD_PTR(service_batch[i][0])) {
		const char **snames = &service_batch[i][0];
		int j = 0;

		if(i != 0) {
			errcode |= service_batch_wait(&service_batch[i-1][0], STAGE_SINCE, SERVICE_INITING, -1);
		}

		while(TEST_VAILD_PTR(snames[j])) {
			const char *sname = snames[j];
			errcode |= service_boost(sname);
			if(!TEST_SUCCESS(errcode)) {
				return errcode;
			}
			++ j;
		}

		if(!TEST_SUCCESS(errcode)) {
			return errcode;
		}
		++ i;
	}

	return errcode;
}

void service_stop(const char *sname, HANDLE service_handle) {
	CHECK_VAILD_PTR(S);
	module *mod = NULL;
	if(TEST_VAILD_PTR(sname)) {
		mod = module_query(sname);
	} else if(TEST_VAILD_SERVICE_HANDLE(service_handle)) {
		mod = service_get_module(service_handle);
	}

	if(TEST_VAILD_PTR(mod)) {
		inner_service_stop(mod, service_handle);
	}
}

HANDLE service_get_handle(const char *sname) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(sname);
	HANDLE handle = INVAILD_SERVICE_HANDLE;
	SPIN_LOCK(S);
	for(int i=0; i<SERVICE_POOL_SIZE-1; ++i) {
		if(TEST_VAILD_PTR(S->services[i]) &&
				0 == strcmp(S->services[i]->name, sname)) {
			handle = i;
			break;
		}
	}

	SPIN_UNLOCK(S);

	return handle;
}

logger *service_get_logger(HANDLE service_handle) {
	CHECK_VAILD_SERVICE_HANDLE(service_handle);
	SPIN_LOCK(S);
	logger *log = NULL;
	service *s = S->services[service_handle];
	if(TEST_VAILD_PTR(s)) {
		log = s->log;
	}
	SPIN_UNLOCK(S);

	return log;
}

uint32_t service_get_harborid(HANDLE handle) {
	return HARBOR_ID(context_get_nodeid(), handle);
}

/* *
 * timeout_s < 0 : 永久等待
 * timeout_s == 0 : 即刻返回
 * timeout_s > 0 : 超时等待
 * */
int service_wait(const char *sname, STAGE_TYPE type,
		service_state state, double timeout_s) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(sname);

	timestamp last = timestamp_now();
	HANDLE handle = INVAILD_SERVICE_HANDLE;

	while(true) {
		handle = service_get_handle(sname);

		if(TEST_VAILD_SERVICE_HANDLE(handle)) {
			break;
		}

		timestamp now = timestamp_now();
		double diff = timestamp_diff(now, last);

		if(timeout_s >= 0 && diff > timeout_s) {
			return ERROR_FAILD;
		}

		int errcode = thread_yield();
		if(!TEST_SUCCESS(errcode)) {
			fprintf(stderr, "thread_yield faild!\n");
		}
	}

	CHECK(INVAILD_SERVICE_HANDLE != handle);
	CHECK_VAILD_PTR(S->services[handle]);

	while(true) {
		bool ok = false;
		switch(type) {
		case STAGE_EQUAL :
			ok = TEST_STAGE(S->services[handle]->state, state);
			break;
		case STAGE_AFTER	 :
			ok = TEST_STAGE_AFTER(S->services[handle]->state, state);
			break;
		case STAGE_SINCE	 :
			ok = TEST_STAGE_SINCE(S->services[handle]->state, state);
			break;
		case STAGE_BEFORE :
			ok = TEST_STAGE_BEFORE(S->services[handle]->state, state);
			break;
		case STAGE_UNTIL :
			ok = TEST_STAGE_UNTIL(S->services[handle]->state, state);
			break;
		default:
			return ERROR_FAILD;
		}

		if(ok) {
			break;
		}

		timestamp now = timestamp_now();
		double diff = timestamp_diff(now, last);

		if(timeout_s >= 0 && diff > timeout_s)
			return ERROR_FAILD;

		int errcode = thread_yield();
		if(!TEST_SUCCESS(errcode)) {
			fprintf(stderr, "thread_yield faild!\n");
		}
	}

	return ERROR_SUCCESS;
}

int service_batch_wait(const char **snames, STAGE_TYPE type,
		service_state state, double timeout_s) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_PTR(snames);

	timestamp last = timestamp_now();
	HANDLE handle = INVAILD_SERVICE_HANDLE;

	int size = 0;
	while(TEST_VAILD_PTR(snames[size])) {
		++ size;
	}

	while(true) {
		const char *sname = NULL;
		int counter = 0;
		for(int i=0; i<size; ++i) {
			sname = snames[i];
			handle = service_get_handle(sname);
			if(TEST_VAILD_SERVICE_HANDLE(handle)) {
				++ counter;
			}
		}

		if(counter == size) {
			break;
		}

		timestamp now = timestamp_now();
		double diff = timestamp_diff(now, last);

		if(timeout_s >= 0 && diff > timeout_s) {
			return ERROR_FAILD;
		}

		int errcode = thread_yield();
		if(!TEST_SUCCESS(errcode)) {
			fprintf(stderr, "thread_yield faild!\n");
		}
	}

	while(true) {
		bool ok = true;
		const char *sname = NULL;
		for(int i=0; i<size; ++i) {
			sname = snames[i];
			handle = service_get_handle(sname);
			CHECK_VAILD_SERVICE_HANDLE(handle);
			switch(type) {
			case STAGE_EQUAL :
				ok &= TEST_STAGE(S->services[handle]->state, state);
				break;
			case STAGE_AFTER	 :
				ok &= TEST_STAGE_AFTER(S->services[handle]->state, state);
				break;
			case STAGE_SINCE	 :
				ok &= TEST_STAGE_SINCE(S->services[handle]->state, state);
				break;
			case STAGE_BEFORE :
				ok &= TEST_STAGE_BEFORE(S->services[handle]->state, state);
				break;
			case STAGE_UNTIL :
				ok &= TEST_STAGE_UNTIL(S->services[handle]->state, state);
				break;
			default:
				return ERROR_FAILD;
			}
		}

		if(ok) {
			break;
		}

		timestamp now = timestamp_now();
		double diff = timestamp_diff(now, last);

		if(timeout_s >= 0 && diff > timeout_s)
			return ERROR_FAILD;

		int errcode = thread_yield();
		if(!TEST_SUCCESS(errcode)) {
			fprintf(stderr, "thread_yield faild!\n");
		}
	}

	return ERROR_SUCCESS;
}

void service_push_message(HANDLE service_handle, message *msg) {
	CHECK_VAILD_PTR(msg);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	SPIN_UNLOCK(S);

	if(!TEST_VAILD_PTR(s)) {
		context_send_mail(INVAILD_SERVICE_HANDLE, msg);
		return;
	}

	switch(s->type) {
	case TYPE_SERVICE:
	case TYPE_SERVLET:
		context_send_mail(service_handle, msg);
		break;
	default :
		fprintf(stderr, "push service type (%d) error!", s->type);
		FREE(msg);
	}
}

message *service_pop_message(HANDLE service_handle) {
	CHECK_VAILD_PTR(S);
	CHECK_VAILD_SERVICE_HANDLE(service_handle);
	SPIN_LOCK(S);
	service *s = S->services[service_handle];
	CHECK_VAILD_PTR(s);
	service_type type = s->type;
	SPIN_UNLOCK(S);

	CHECK(type >= TYPE_SERVICE && type <= TYPE_SERVLET);
	message *msg = NULL;

	switch(type) {
	case TYPE_SERVICE:
		/* worker线程不可以阻塞 */
		msg = context_try_recv_mail(service_handle);
		break;
	case TYPE_SERVLET:
		/* 可能会阻塞 */
		msg = context_recv_mail(service_handle);
		break;
	default:
		fprintf(stderr, "pop service type (%d) error!", s->type);
		break;
	}

	return msg;
}

void service_handle_trash() {
	CHECK_VAILD_PTR(S);
	message *msg = context_try_recv_mail(INVAILD_SERVICE_HANDLE);
	/* 注意：不能转为msg后再判断指针的有效性，node为NULL，msg不为NULL */
	if(TEST_VAILD_PTR(msg)) {
		if(TEST_VAILD_PTR(msg->data)) {
			FREE(msg->data);
		}
		FREE(msg);
	}
}

static void *inner_dispatch_routine(void *input, int *errcode) {
	int handle = *(HANDLE *)input;
	printf("dispatch message service handle is %d\n", handle);
	CHECK_VAILD_SERVICE_HANDLE(handle);
	module *mod = service_get_module(handle);
	CHECK_VAILD_PTR(mod);
	*errcode = ERROR_FAILD;
	if(TEST_VAILD_PTR(mod)) {
		*errcode = inner_service_signal(mod, handle, SIG_SERVICE_START);
	}

	return NULL;
}

void service_dispatch_message() {
	service *s = NULL;
	threadpool_task *task;

	for(int i=0; i<SERVICE_POOL_SIZE-1; ++i) {
		s = S->services[i];

		if(TEST_VAILD_PTR(s) && TYPE_SERVLET == s->type && context_has_mail(i)) {
			task = threadpool_task_create(inner_dispatch_routine, &s->handle, NULL);
			threadpool_submit(task);
		}
	}

}
