/*
 * module.c
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#include <stdio.h>
#include <dlfcn.h>

#include <errcode.h>
#include <spinlock.h>
#include <env.h>
#include <context.h>
#include <module.h>

#define MAX_MODULE_TYPE 32

typedef int (*dl_signal)(HANDLE service_handle, signalno no);

typedef struct module {
	const char * name;
	void * module;
	dl_signal signal;
} module;

typedef struct modules {
	int count;
	spinlock lock;
	char * path;
	module mods[MAX_MODULE_TYPE];
} modules;

static modules *M = NULL;

int module_init() {
	int errcode = ERROR_FAILD;
	const char *path = env_get("module_path");
	CHECK_VAILD_PTR(path);
	MALLOC_DEF(m, modules);
	if(TEST_VAILD_PTR(m)) {
		m->count = 0;
		m->path = strdup(path);
		CHECK_VAILD_PTR(m->path);
		SPIN_INIT(m);
		M = m;
		errcode = ERROR_SUCCESS;
	}

	return errcode;
}

void module_release() {
	if(TEST_VAILD_PTR(M)) {
		SPIN_LOCK(M);
		FREE(M->path);
		SPIN_UNLOCK(M);
		SPIN_DESTROY(M);
		FREE(M);
	}
}

static void *inner_try_open(modules *mods, const char *name) {
	CHECK_VAILD_PTR(mods);
	CHECK_VAILD_PTR(name);
	const char *l;
	const char * path = mods->path;
	size_t path_size = strlen(path);
	size_t name_size = strlen(name);
#ifdef __linux__
	int sz = path_size + name_size + 3;
#elif defined (__APPLE__)
	int sz = path_size + name_size + 6;
#endif
	//search path
	void * dl = NULL;
	char tmp[sz];
	do
	{
		memset(tmp,0,sz);
		while (*path == ';') {
			++ path;
		}

		if (*path == '\0') {
			break;
		}

		l = strchr(path, ';');

		if (l == NULL) {
			l = path + strlen(path);
		}

		int len = l - path;
		int i;

		for (i=0; path[i]!='?' && i < len ; i++) {
			tmp[i] = path[i];
		}

		memcpy(tmp+i,name,name_size);
		if (path[i] == '?') {
			strncpy(tmp+i+name_size, path+i+1, len - i - 1);
#ifdef __linux__
			strncpy(tmp+len-1+name_size, ".so", 3);
#elif defined (__APPLE__)
			strncpy(tmp+len-1+name_size, ".dylib", 6);
#endif
		} else {
			fprintf(stderr,"Invalid C service path\n");
			exit(1);
		}

		dl = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
		path = l;
	}while(dl == NULL);

	if (dl == NULL) {
		fprintf(stderr, "try open %s failed : %s\n",name,dlerror());
	}

	return dl;
}

static module *inner_query(const char * name) {
	CHECK_VAILD_PTR(name);
	int i;
	for (i=0; i<M->count; i++) {
		if (strcmp(M->mods[i].name, name)==0) {
			return &M->mods[i];
		}
	}

	return NULL;
}

static void *inner_get_api(module *mod, const char *api_name) {
	CHECK_VAILD_PTR(mod);
	CHECK_VAILD_PTR(api_name);
	size_t name_size = strlen(mod->name);
	size_t api_size = strlen(api_name);
	char tmp[name_size + api_size + 2];
	memcpy(tmp, mod->name, name_size);
	tmp[name_size] = '_';
	memcpy(tmp + name_size + 1, api_name, api_size + 1);

	return dlsym(mod->module, tmp);
}

static int inner_open_sym(module *mod) {
	CHECK_VAILD_PTR(mod);
	mod->signal = inner_get_api(mod, "signal");
	if(!TEST_VAILD_PTR(mod->signal)) {
		fprintf(stderr, "try get symbal failed : %s\n", dlerror());
	}
	return mod->signal == NULL;
}

module *module_query(const char * name) {
	CHECK_VAILD_PTR(name);
	module * result = inner_query(name);
	if (result)
		return result;

	SPIN_LOCK(M);
	result = inner_query(name); // double check

	if (!TEST_VAILD_PTR(result) && M->count < MAX_MODULE_TYPE) {
		int index = M->count;
		void * dl = inner_try_open(M,name);
		if (dl) {
			M->mods[index].name = name;
			M->mods[index].module = dl;

			if (inner_open_sym(&M->mods[index]) == 0) {
				M->mods[index].name = strdup(name);
				M->count ++;
				result = &M->mods[index];
			}
		}
	}

	SPIN_UNLOCK(M);

	return result;
}

void module_insert(module *mod) {
	CHECK_VAILD_PTR(mod);
	SPIN_LOCK(M);

	module * m = inner_query(mod->name);
	CHECK(!TEST_VAILD_PTR(m) && M->count < MAX_MODULE_TYPE);
	int index = M->count;
	M->mods[index] = *mod;
	++ M->count;

	SPIN_UNLOCK(M);
}

int module_signal(module *m, HANDLE service_handle, signalno no) {
	int errcode = ERROR_FAILD;
	CHECK_VAILD_PTR(m);

	if (m->signal) {
		// printf("before service %s signal(%d)\n", m->name, no);
		errcode = m->signal(service_handle, no);
		// printf("after service %s signal(%d)\n", m->name, no);
	}

	return errcode;
}
