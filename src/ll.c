#include <ll.h>
#include <string.h>

ll_t *ll_create(allocator *al) {
	if (!al)
		return NULL;

	ll_t *ll = alloc(al, sizeof(ll_t));

	if (!ll)
		return NULL;

	memset(ll, 0, sizeof(ll));

	return ll;
}

void ll_destroy(allocator *al, ll_t *ll) {
	if (!al || !ll)
		return;

	size_t size = ll_size(ll);
	
	ll_node_t *node = ll->first;
	for (size_t i = 0; i < size; i++) {
		free(al, node);
		// we can still use this,
		// since we own the allocator
		// the whole function 
		// meaning no one can access 
		// the space we just freed
		node = node->next;
	}

	free(al, ll);
}

size_t ll_size(ll_t *ll) {
	if (!ll)
		return 0;

	return ll->size;
}

void *ll_get(ll_t *ll, size_t index) {
	if (!ll || index >= ll_size(ll))
		return NULL;

	ll_node_t *node = ll->first;
	for (size_t i = 0; i < index; i++) {
		node = node->next;
	}

	return node->content;
}

int ll_add_back(allocator *al, ll_t *ll, void *content) {
	if (!al || !ll)
		return -1;

	ll_node_t *node = alloc(al, sizeof(ll_node_t));

	if (!node)
		return -1;

	node->content = content;
	node->next = NULL;
	node->prev = ll->last;

	ll->last = node;
	ll->size++;
	
	return 0;
}

int ll_add_front(allocator *al, ll_t *ll, void *content) {
	if (!al || !ll)
		return -1;

	ll_node_t *node = alloc(al, sizeof(ll_node_t));

	if (!node)
		return -1;

	node->content = content;
	node->prev = NULL;
	node->next = ll->first;

	ll->first = node;
	ll->size++;

	return 0;
}

void ll_remove(allocator *al, ll_t *ll, size_t index) {
	if (!al || !ll || index >= ll_size(ll))
		return;

	ll_node_t *node = ll->first;
	for (size_t i = 0; i < index; i++) {
		node = node->next;
	}

	if (node->prev)
		node->prev->next = node->next;
	else
		ll->first = node->next;

	if (node->next)
		node->next->prev = node->prev;
	else
		ll->last = node->prev;

	ll->size--;

	free(al, node);
}

void ll_remove_pointer(allocator *al, ll_t *ll, void *content) {
	if (!al || !ll)
		return;

	size_t size = ll_size(ll);
	ll_node_t *node =  ll->first;
	for (size_t i = 0; i < size; i++) {
		if (node->content == content)
			goto found;
	}
	return;

found:
	if (node->prev)
		node->prev->next = node->next;
	else
		ll->first = node->next;

	if (node->next)
		node->next->prev = node->prev;
	else
		ll->last = node->prev;

	ll->size--;

	free(al, node);
}
