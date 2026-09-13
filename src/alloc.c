#include <alloc.h>

void new_allocator(uintptr_t addr, size_t size, allocator *alloc)
{
	heap_node *node = (heap_node *)addr;

	node->prev = NULL;
	node->next = NULL;
	node->status = FREE;
	node->size = size - sizeof(heap_node);

	alloc->first = node;
	alloc->addr = addr;
	alloc->size = size;
}

void *alloc(allocator *alloc, size_t size)
{
	if (size < 0x20) size = 0x20;

	heap_node *cur_node = alloc->first;

	while (cur_node != NULL) {
		if (cur_node->size == size && cur_node->status == FREE) {
			cur_node->status = USED;
			return (void *)(cur_node + 1);
		}
		if (cur_node->size >= (size + sizeof(heap_node) + 0x20)
		    && cur_node->status == FREE) {
			heap_node *new_node = (heap_node *)((uintptr_t)cur_node + size + sizeof(
			                heap_node));
			new_node->size = cur_node->size - size - sizeof(heap_node);
			new_node->status = FREE;
			new_node->prev = cur_node;
			new_node->next = cur_node->next;
			if (cur_node->next != NULL) {
				cur_node->next->prev = new_node;
			}
			cur_node->next = new_node;
			cur_node->status = USED;
			cur_node->size = size;
			return (void *)(cur_node + 1);
		}
		cur_node = cur_node->next;
	}

	return 0;
}

void free(allocator *alloc, void *ptr)
{
	if (ptr == NULL) return;
	if ((uintptr_t)ptr < alloc->addr
	    || (uintptr_t)ptr >= alloc->addr + alloc->size) return;

	heap_node *node = (heap_node *)((uint8_t *)ptr - sizeof(heap_node));

	if (node->status == FREE) return;

	node->status = FREE;

	heap_node *prev = node->prev;
	heap_node *next = node->next;

	if (prev != NULL && prev->status == FREE) {
		prev->size = prev->size + node->size + sizeof(heap_node);
		prev->next = next;
		if (next != NULL) {
			next->prev = prev;
		}
		node = prev;
	}

	if (next != NULL && next->status == FREE) {
		node->size = next->size + node->size + sizeof(heap_node);
		node->next = next->next;
		if (node->next != NULL) {
			node->next->prev = node;
		}
	}

}
