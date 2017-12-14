/*
 * env.h
 *
 *  Created on: 2017年11月8日
 *      Author: linzer
 */

#ifndef __QNODE_ENV_H__
#define __QNODE_ENV_H__

#include <define.h>

int env_init(const char *file);
void env_import_config(const char *file);
void env_set(const char *key, const char *value);
const char *env_get(const char *key);
void env_del(const char *key);
void env_release();

#endif /* __QNODE_ENV_H__ */
