/*
 * module.h
 *
 *  Created on: 2017年10月31日
 *      Author: linzer
 */

#ifndef __QNODE_MODULE_H__
#define __QNODE_MODULE_H__

#include <define.h>
#include <service.h>

FORWARD_DECLAR(module)
FORWARD_DECLAR(context)

int module_init();
void module_release();
module *module_query(const char * name);
void module_insert(module *mod);
int module_signal(module *m, HANDLE service_handle, signalno no);


#endif /* __QNODE_MODULE_H__ */
