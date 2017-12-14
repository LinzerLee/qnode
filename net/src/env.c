/*
 * env.c
 *
 *  Created on: 2017年11月8日
 *      Author: linzer
 */

#include <string.h>
#include <ctype.h>

#include	 <tire.h>
#include <util.h>
#include <env.h>

typedef struct env{
	tiretree tree;
	spinlock lock;
} env;

static env *E = NULL;

static bool inner_check_config(char *line, const char **key, const char **value) {
	char *cur = strchr(line, '=');
	char *end = line + strlen(line) - 1;
	if(TEST_VAILD_PTR(cur)) {
		*cur = '\0';
		char *start = line;
		char *left = cur - 1;
		char *right = cur + 1;

		while(start <= left && isspace(*start)) {
			*start = '\0';
			++ start;
		}

		while(end >= right && isspace(*end)) {
			*end = '\0';
			-- end;
		}

		if(start > left || end < right)
			return false;

		while(isspace(*left)) {
			*left = '\0';
			-- left;
		}

		while(isspace(*right)) {
			*right = '\0';
			++ right;
		}

		*key = start;
		*value = right;

		return true;
	}

	return false;
}

static void inner_import_config(tiretree *tree, const char *file) {
	CHECK_VAILD_PTR(tree);
	smallfile *sf = smallfile_open(file);
	CHECK_VAILD_PTR(sf);
	smallfile_read(sf);
	const char *delim = "\n";
	char *token = strtok(sf->buf, delim);
	const char *key, *value;
	while(TEST_VAILD_PTR(token)) {
		if(inner_check_config(token, &key, &value)) {
			char *v = strdup(value);
			CHECK_VAILD_PTR(v);
			tiretree_insert(tree, key, v);
		}
		token = strtok(NULL, delim);
	}
	smallfile_close(sf);
}

void env_import_config(const char *file) {
	CHECK_VAILD_PTR(E);
	SPIN_LOCK(E);
	inner_import_config(&E->tree, file);
	SPIN_UNLOCK(E);
}

int env_init(const char *file) {
	int errcode = ERROR_FAILD;
	MALLOC_DEF(e, env);
	if(TEST_VAILD_PTR(e)) {
		SPIN_INIT(e);
		tiretree_init(&e->tree);
		E = e;
		errcode = ERROR_SUCCESS;
	}

	if(TEST_VAILD_PTR(file)) {
		env_import_config(file);
	}
	return errcode;
}

void env_set(const char *key, const char *value) {
	CHECK_VAILD_PTR(E);
	SPIN_LOCK(E);
	char *v = strdup(value);
	CHECK_VAILD_PTR(v);
	tiretree_insert(&E->tree, key, v);
	SPIN_UNLOCK(E);
}

const char *env_get(const char *key) {
	CHECK_VAILD_PTR(E);
	SPIN_LOCK(E);
	const char *v = tiretree_query(&E->tree, key);
	SPIN_UNLOCK(E);

	return v;
}

void env_del(const char *key) {
	CHECK_VAILD_PTR(E);
	SPIN_LOCK(E);
	void *v = tiretree_remove(&E->tree, key);
	FREE(v);
	SPIN_UNLOCK(E);
}

void env_release() {
	CHECK_VAILD_PTR(E);
	SPIN_LOCK(E);
	tiretree_release(&E->tree);
	SPIN_UNLOCK(E);
	SPIN_DESTROY(E);
	FREE(E);
}
