#ifndef _LL_H
#define _LL_H

//
// double linked list implementation
//

#include <alloc.h>
#include <stdint.h>
#include <sem.h>

typedef struct ll_node_t {
	void *content;

	struct ll_node_t *next;
	struct ll_node_t *prev;
} ll_node_t;

typedef struct ll_t {
	ll_node_t *first;
	ll_node_t *last;

	size_t size;
} ll_t;

// you can also just create an empty struct
// and use that like done in fs/vfs.c
ll_t *ll_create(allocator *al);

// all the node are freed automatically
// but the content pointers are lost
// so make sure to free all of them
void ll_destroy(allocator *al, ll_t *ll);

size_t ll_size(ll_t *ll);
void *ll_get(ll_t *ll, size_t index);

int ll_add_back(allocator *al, ll_t *ll, void *content);
int ll_add_front(allocator *al, ll_t *ll, void *content);

void ll_remove(allocator *al, ll_t *ll, size_t index);
void ll_remove_pointer(allocator *al, ll_t *ll, void *content);

DEFINE_MUTEX_TYPE(ll_t);

#endif // _LL_H
