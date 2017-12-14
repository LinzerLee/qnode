/*
 * heap.c
 *
 *  Created on: 2017年10月25日
 *      Author: linzer
 */

#include <heap.h>

void heap_init(heap* heap, heap_compare_fn less_than) {
	heap->min = NULL;
	heap->nelts = 0;
	heap->less_than = less_than;
}

heap_node* heap_min(const heap* heap) {
	return heap->min;
}

/* Swap parent with child. Child moves closer to the root, parent moves away. */
static inline void heap_node_swap(heap* heap, heap_node* parent, heap_node* child) {
	heap_node* sibling;
	heap_node t;

	t = *parent;
	*parent = *child;
	*child = t;

	parent->parent = child;
	if (child->left == child) {
		child->left = parent;
		sibling = child->right;
	} else {
		child->right = parent;
		sibling = child->left;
	}

	if (sibling != NULL)
		sibling->parent = child;

	if (parent->left != NULL)
		parent->left->parent = parent;
	if (parent->right != NULL)
		parent->right->parent = parent;

	if (child->parent == NULL)
		heap->min = child;
	else if (child->parent->left == parent)
		child->parent->left = child;
	else
		child->parent->right = child;
}

void heap_insert(heap* heap, heap_node* newnode) {
	heap_node** parent;
	heap_node** child;
	unsigned int path;
	unsigned int n;
	unsigned int k;

	newnode->left = NULL;
	newnode->right = NULL;
	newnode->parent = NULL;

	/* Calculate the path from the root to the insertion point.  This is a min
	 * heap so we always insert at the left-most free node of the bottom row.
	 */
	path = 0;
	for (k = 0, n = 1 + heap->nelts; n >= 2; k += 1, n /= 2)
		path = (path << 1) | (n & 1);

	/* Now traverse the heap using the path we calculated in the previous step. */
	parent = child = &heap->min;
	while (k > 0) {
		parent = child;
		if (path & 1)
			child = &(*child)->right;
		else
			child = &(*child)->left;
		path >>= 1;
		k -= 1;
	}

	/* Insert the new node. */
	newnode->parent = *parent;
	*child = newnode;
	heap->nelts += 1;

	/* Walk up the tree and check at each node if the heap property holds.
	 * It's a min heap so parent < child must be true.
	 */
	while (newnode->parent != NULL && heap->less_than(newnode, newnode->parent))
		heap_node_swap(heap, newnode->parent, newnode);
}

void heap_remove(heap* heap, heap_node* node) {
	heap_node* smallest;
	heap_node** max;
	heap_node* child;
	unsigned int path;
	unsigned int k;
	unsigned int n;

	if (heap->nelts == 0)
		return;

	/* Calculate the path from the min (the root) to the max, the left-most node
	 * of the bottom row.
	 */
	path = 0;
	for (k = 0, n = heap->nelts; n >= 2; k += 1, n /= 2)
		path = (path << 1) | (n & 1);

	/* Now traverse the heap using the path we calculated in the previous step. */
	max = &heap->min;
	while (k > 0) {
		if (path & 1)
			max = &(*max)->right;
		else
			max = &(*max)->left;
		path >>= 1;
		k -= 1;
	}

	heap->nelts -= 1;

	/* Unlink the max node. */
	child = *max;
	*max = NULL;

	if (child == node) {
		/* We're removing either the max or the last node in the tree. */
		if (child == heap->min) {
			heap->min = NULL;
		}
		return;
	}

	/* Replace the to be deleted node with the max node. */
	child->left = node->left;
	child->right = node->right;
	child->parent = node->parent;

	if (child->left != NULL) {
		child->left->parent = child;
	}

	if (child->right != NULL) {
		child->right->parent = child;
	}

	if (node->parent == NULL) {
		heap->min = child;
	} else if (node->parent->left == node) {
		node->parent->left = child;
	} else {
		node->parent->right = child;
	}

	/* Walk down the subtree and check at each node if the heap property holds.
	 * It's a min heap so parent < child must be true.  If the parent is bigger,
	 * swap it with the smallest child.
	 */
	for (;;) {
		smallest = child;
		if (child->left != NULL && heap->less_than(child->left, smallest))
			smallest = child->left;
		if (child->right != NULL && heap->less_than(child->right, smallest))
			smallest = child->right;
		if (smallest == child)
			break;
		heap_node_swap(heap, child, smallest);
	}

	/* Walk up the subtree and check that each parent is less than the node
	 * this is required, because `max` node is not guaranteed to be the
	 * actual maximum in tree
	 */
	while (child->parent != NULL && heap->less_than(child, child->parent))
		heap_node_swap(heap, child->parent, child);
}

void heap_dequeue(heap* heap) {
	heap_remove(heap, heap->min);
}
