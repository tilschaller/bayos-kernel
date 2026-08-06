#ifndef _ALLOC_H
#define _ALLOC_H

#include <stdint.h>
#include <sched.h>

typedef enum {
	USED,
	FREE,
} heap_node_status;

typedef struct heap_node {
	size_t size;
	heap_node_status status;
	struct heap_node *prev;
	struct heap_node *next;
} heap_node;

typedef struct allocator {
	heap_node *first;
	uintptr_t addr;
	size_t size;
} allocator;

void new_allocator(uintptr_t addr, size_t size, allocator *alloc);

void *alloc(allocator *alloc, size_t size);
void free(allocator *alloc, void *ptr);

DEFINE_MUTEX_TYPE(allocator);
extern Mutex(allocator) g_al;

#endif // _ALLOC_H
