/*
 * tire.c
 *
 *  Created on: 2017年11月7日
 *      Author: linzer
 */

#include <tire.h>

// static const char alpha[] = "._0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
// STATIC_ASSERT(ALPHA_SIZE + 1 == sizeof alpha, alpha数组大小不等于ALPHA_SIZE);

/* Public functions. */
void tiretree_init(tiretree *tree) {
	NUL(tree->tree_head.value);
	ZERO(tree->tree_head.next);
	tree->tree_head.ref = 0;
}

static bool tiretree_check_key(const char *key) {
	const char *ptr = key;
	char ch;
	while(*ptr) {
		ch = *ptr;
		if(ch == '.' || ch == '_' || (ch >= '0' && ch <= '9') ||
				(ch >= 'a' && ch <= 'z')  || (ch >= 'A' && ch <= 'Z')) {
			++ ptr;
		} else {
			return false;
		}
	}

	return true;
}

static int tiretree_alpha2index(char ch) {
	if(ch == '0')
		return 0;

	if(ch == '_')
		return 1;

	if(ch >= '0' && ch <= '9')
		return ch - '0' + 2;

	if(ch >= 'a' && ch <= 'z')
		return ch - 'a' + 12;

	if(ch >= 'A' && ch <= 'Z')
		return ch - 'A' + 38;

	return -1;
}

bool tiretree_insert(tiretree *tree, const char *key, void *value) {
	bool ret = tiretree_check_key(key);
	tirenode *node = &tree->tree_head;
	if(ret) {
		const char *ptr = key;
		int index;
		while(*ptr) {
			index = tiretree_alpha2index(*ptr);
			CHECK(index != -1);
			if(!TEST_VAILD_PTR(node->next[index])) {
				MALLOC_DEF(newnode, tirenode);
				CHECK_VAILD_PTR(newnode);
				ZERO(newnode->next);
				NUL(newnode->value);
				newnode->ref = 0;
				node->next[index] = newnode;
				++ node->ref;
			}

			node = node->next[index];
			++ ptr;
		}

		node->value = value;
		++ node->ref;
		return true;
	}

	return false;
}

void *tiretree_query(tiretree *tree, const char *key) {
	void *value = NULL;
	tirenode *node = &tree->tree_head;
	tirenode *next = NULL;
	if(tiretree_check_key(key)) {
		const char *ptr = key;
		int index;
		while(*ptr) {
			index = tiretree_alpha2index(*ptr);
			CHECK(index != -1);
			next = node->next[index];

			if(!TEST_VAILD_PTR(next)) {
				return NULL;
			}

			node = next;
			++ ptr;
		}

		value = node->value;
	}

	return value;
}

void *tiretree_remove(tiretree *tree, const char *key) {
	void *value = tiretree_query(tree, key);
	if(TEST_VAILD_PTR(value)) {
		tirenode *node = &tree->tree_head;
		tirenode *next = NULL;
		const char *ptr = key;
		int index;
		int stack_size = sizeof(tirenode **) * (strlen(key) + 1);
		tirenode ***stack = (tirenode ***)malloc(stack_size);
		int depth = 0;
		CHECK_VAILD_PTR(stack);
		memset(stack, 0, stack_size);
		while(*ptr) {
			index = tiretree_alpha2index(*ptr);
			CHECK(index != -1);
			next = node->next[index];
			CHECK_VAILD_PTR(next);

			stack[++ depth] = &node->next[index];
			node = next;
			++ ptr;
		}

		NUL((*stack[depth])->value);

		while(depth > 0) {
			tirenode **node = stack[depth];
			if(0 == (-- (*node)->ref)) {
				FREE(*node);
				-- depth;
			} else {
				break;
			}
		}

		free(stack);
	}

	return value;
}

static void inner_node_release(tirenode *node) {
	if(!TEST_VAILD_PTR(node))
		return;

	for(int i=0; i<ALPHA_SIZE; ++i) {
		if(TEST_VAILD_PTR(node->next[i])) {
			inner_node_release(node->next[i]);
		}
	}

	FREE(node);
}

void tiretree_release(tiretree *tree) {
	if(!TEST_VAILD_PTR(tree))
		return;

	for(int i=0; i<ALPHA_SIZE; ++i) {
		if(TEST_VAILD_PTR(tree->tree_head.next[i])) {
			inner_node_release(tree->tree_head.next[i]);
		}
	}

	tiretree_init(tree);
}

/* id tree */
void idtree_init(idtree *tree) {
	NUL(tree->tree_head.value);
	ZERO(tree->tree_head.next);
	tree->tree_head.ref = 0;
}

static inline char MASK_HALF_BYTE(key, n)	 {
	uint64_t k = key;
	k >>= (15 - n) << 2;
	k &= 0x000000000000000F;

	return k;
}

bool idtree_insert(idtree *tree, uint64_t key, void *value) {
	idnode *node = &tree->tree_head;
	char arr[16] = { 0 };
	for(int i=0; i<16; ++i) {
		arr[i] = MASK_HALF_BYTE(key, i);
	}

	for(int i=0; i<16; ++i) {
		int index = arr[i];
		if(!TEST_VAILD_PTR(node->next[index])) {
			MALLOC_DEF(newnode, idnode);
			CHECK_VAILD_PTR(newnode);
			ZERO(newnode->next);
			NUL(newnode->value);
			newnode->ref = 0;
			node->next[index] = newnode;
			++ node->ref;
		}
		node = node->next[index];
	}

	node->value = value;
	++ node->ref;
	return true;
}

void *idtree_query(idtree *tree, uint64_t key) {
	void *value = NULL;
	idnode *node = &tree->tree_head;
	int index = 0;

	for(int i=0; i<16; ++i) {
		index = MASK_HALF_BYTE(key, i);
		node = node->next[index];

		if(!TEST_VAILD_PTR(node)) {
			break;
		}
	}

	if(TEST_VAILD_PTR(node)) {
		value = node->value;
	}

	return value;
}

void *idtree_remove(idtree *tree, uint64_t key) {
	void *value = idtree_query(tree, key);
	if(TEST_VAILD_PTR(value)) {
		idnode *node = &tree->tree_head;
		idnode *next = NULL;
		int index;
		idnode **stack[16] = { NULL };
		int depth = 0;
		for(int i=0; i<16; ++i) {
			index = MASK_HALF_BYTE(key, i);
			next = node->next[index];
			CHECK_VAILD_PTR(next);
			stack[++ depth] = &node->next[index];
			node = next;
		}

		NUL((*stack[depth])->value);

		while(depth > 0) {
			idnode **node = stack[depth];
			if(0 == (-- (*node)->ref)) {
				FREE(*node);
				-- depth;
			} else {
				break;
			}
		}
	}

	return value;
}

static void inner_idnode_release(idnode *node) {
	if(!TEST_VAILD_PTR(node))
		return;

	for(int i=0; i<16; ++i) {
		if(TEST_VAILD_PTR(node->next[i])) {
			inner_idnode_release(node->next[i]);
		}
	}

	FREE(node);
}

void idtree_release(idtree *tree) {
	if(!TEST_VAILD_PTR(tree))
		return;

	for(int i=0; i<16; ++i) {
		if(TEST_VAILD_PTR(tree->tree_head.next[i])) {
			inner_idnode_release(tree->tree_head.next[i]);
		}
	}

	idtree_init(tree);
}
