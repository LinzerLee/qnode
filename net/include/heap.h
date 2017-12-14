/*
 * heap.h
 *
 *  Created on: 2017年10月25日
 *      Author: linzer
 */

#ifndef __QNODE_HEAP_H__
#define __QNODE_HEAP_H__

#include <stddef.h>  /* NULL */

#include <define.h>
FORWARD_DECLAR(heap_node)

typedef struct heap_node{
  heap_node *left;
  heap_node *right;
  heap_node *parent;
} heap_node;

/* A binary min heap.  The usual properties hold: the root is the lowest
 * element in the set, the height of the tree is at most log2(nodes) and
 * it's always a complete binary tree.
 */

/* Return non-zero if a < b. */
typedef int (* heap_compare_fn)(const heap_node *a, const heap_node *b);

typedef struct {
  heap_node *min;
  unsigned int nelts;
  heap_compare_fn less_than;
} heap;

/* Public functions. */
void heap_init(heap *heap, heap_compare_fn cmpfn);
struct heap_node* heap_min(const heap *heap);
void heap_insert(heap *heap, heap_node *newnode);
void heap_remove(heap *heap, heap_node *node);
void heap_dequeue(heap *heap);

#endif /* __QNODE_HEAP_H__ */
