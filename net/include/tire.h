/*
 * tire.h
 *
 *  Created on: 2017年11月7日
 *      Author: linzer
 */

#ifndef __QNODE_TIRE_H__
#define __QNODE_TIRE_H__

#include <define.h>

#define ALPHA_SIZE	64

FORWARD_DECLAR(tirenode)
typedef struct tirenode{
	tirenode *next[ALPHA_SIZE];
	void *value;
	char ref;	/* 引用计数=有效子节点个数+当前节点是否有数据(1 | 0) */
} tirenode;

typedef struct tiretree {
	tirenode tree_head;
} tiretree;

/* Public functions. */
void tiretree_init(tiretree *tree);
bool tiretree_insert(tiretree *tree, const char *key, void *value);
void *tiretree_remove(tiretree *tree, const char *key);
void *tiretree_query(tiretree *tree, const char *key);
void tiretree_release(tiretree *tree);

FORWARD_DECLAR(idnode)
typedef struct idnode{
	idnode *next[16];
	void *value;
	char ref;	/* 引用计数=有效子节点个数+当前节点是否有数据(1 | 0) */
} idnode;

typedef struct idtree {
	idnode tree_head;
} idtree;

/* Public functions. */
void idtree_init(idtree *tree);
bool idtree_insert(idtree *tree, uint64_t key, void *value);
void *idtree_remove(idtree *tree, uint64_t key);
void *idtree_query(idtree *tree, uint64_t key);
void idtree_release(idtree *tree);

#endif /* __QNODE_TIRE_H__ */
