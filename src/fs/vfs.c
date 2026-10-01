#include <fs/vfs.h>
#include <ll.h>
#include <alloc.h>

Mutex(ll_t) mountpoint_list;

void vfs_init(void) {
	allocator *al = MUTEX_LOCK(g_al);
	mountpoint_list.data = ll_create(al);
	mutex *mut = alloc(al, sizeof(mutex));
	MUTEX_UNLOCK(g_al);

	mountpoint_list.lock = mutex_create(mut);
}
