/*
 *  文件：list.h
 *  时间: 2017年10月17日
 *  作者: linzer lee
 *  描述：双向循环链表
 */

#ifndef __QNODE_LIST_H__
#define __QNODE_LIST_H__

#include <stddef.h>

typedef struct {
	void *next;
	void *prev;
} dclist_node;

typedef dclist_node DCLIST;
typedef dclist_node DCLIST_NODE;

/* 私有宏. */
#define DCLIST_NEXT(ptr)       (*(dclist_node **)&(ptr)->next)
#define DCLIST_PREV(ptr)       (*(dclist_node **)&(ptr)->prev)
#define DCLIST_PREV_NEXT(ptr)  (DCLIST_NEXT(DCLIST_PREV(ptr)))
#define DCLIST_NEXT_PREV(ptr)  (DCLIST_PREV(DCLIST_NEXT(ptr)))

/* 公有宏. */
/* 在遍历时，不要同时对链表进行插入、删除操作，否则会出现未知错误 */
#define DCLIST_FOREACH(ptr, head)                                             \
  for ((ptr) = DCLIST_NEXT(head); (ptr) != (head); (ptr) = DCLIST_NEXT(ptr))

#define DCLIST_EMPTY(head)													  \
  ((const dclist_node *)(head) == (const dclist_node *)DCLIST_NEXT(head))

/* 获取链表中的首节点（第一个数据节点），并不是head指向的头节点 */
#define DCLIST_HEAD(head)                                                     \
  (DCLIST_NEXT(head))

#define DCLIST_TAIL(head)                                                     \
  (DCLIST_PREV(head))

#define DCLIST_INIT(node)                                                     \
  do {                                                                        \
	DCLIST_NEXT(node) = (node);                                               \
	DCLIST_PREV(node) = (node);                                               \
  }                                                                           \
  while (0)

#define DCLIST_ADD(head, other)                                               \
  do {                                                                        \
	DCLIST_PREV_NEXT(head) = DCLIST_NEXT(other);                              \
	DCLIST_NEXT_PREV(other) = DCLIST_PREV(head);                              \
	DCLIST_PREV(head) = DCLIST_PREV(other);                                   \
	DCLIST_PREV_NEXT(head) = (head);                                          \
  }                                                                           \
  while (0)

#define DCLIST_SPLIT(head, split, other)                                      \
  do {                                                                        \
    DCLIST_PREV(other) = DCLIST_PREV(head);                                   \
    DCLIST_PREV_NEXT(other) = (other);                                        \
    DCLIST_NEXT(other) = (split);                                             \
    DCLIST_PREV(head) = DCLIST_PREV(split);                                   \
    DCLIST_PREV_NEXT(head) = (head);                                          \
    DCLIST_PREV(split) = (other);                                             \
  }                                                                           \
  while (0)

#define DCLIST_MOVE(head, other)                                              \
  do {                                                                        \
    if (DCLIST_EMPTY(head))                                                   \
      DCLIST_INIT(other);                                                     \
    else {                                                                    \
      DCLIST* split = DCLIST_HEAD(head);                                      \
      DCLIST_SPLIT(head, split, other);                                       \
    }                                                                         \
  }                                                                           \
  while (0)

#define DCLIST_INSERT_HEAD(head, node)                                        \
  do {                                                                        \
    DCLIST_NEXT(node) = DCLIST_NEXT(head);                                    \
    DCLIST_PREV(node) = (head);                                               \
    DCLIST_NEXT_PREV(node) = (node);                                          \
    DCLIST_NEXT(head) = (node);                                               \
  }                                                                           \
  while (0)

#define DCLIST_INSERT_TAIL(head, node)                                        \
  do {                                                                        \
    DCLIST_NEXT(node) = (head);                                               \
    DCLIST_PREV(node) = DCLIST_PREV(head);                                    \
    DCLIST_PREV_NEXT(node) = (node);                                          \
    DCLIST_PREV(head) = (node);                                               \
  }                                                                           \
  while (0)

#define DCLIST_REMOVE(node)                                                   \
  do {                                                                        \
    DCLIST_PREV_NEXT(node) = DCLIST_NEXT(node);                               \
    DCLIST_NEXT_PREV(node) = DCLIST_PREV(node);                               \
  }                                                                           \
  while (0)
#endif /* __QNODE_LIST_H__ */
