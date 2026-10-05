#include <fs/vfs.h>
#include <ll.h>
#include <alloc.h>
#include <string.h>
#include <printk.h>

Mutex(ll_t) mountpoint_list;

// if two mutexes are needed, it is
// neccessary to lock the mutexes in the same order
// to prevent deadlocks. the order used here is
// 	MUTEX_UNLOCK(mountpoint_list)
// 	MUTEX_LOCK(g_al)
// 	... // do something with these mutexes
// 	MUTEX_UNLOCK(g_al)
// 	MUTEX_UNLOCK(mountpoint_list)

int vfs_mount(device_t *device, const char *path, fs_operations_t *operations) {
	allocator *al = MUTEX_LOCK(g_al);
	mountpoint_t *mountpoint = alloc(al, sizeof(mountpoint_t));
	MUTEX_UNLOCK(g_al);

	if (!mountpoint)
		return -1;

	mountpoint->device = device;
	mountpoint->operations = operations;

	al = MUTEX_LOCK(g_al);
	mountpoint->path = alloc(al, strlen(path) + 1);
	MUTEX_UNLOCK(g_al);

	if (!mountpoint->path) {
		al = MUTEX_LOCK(g_al);
		free(al, mountpoint);
		MUTEX_UNLOCK(g_al);
		return -1;
	}

	strcpy(mountpoint->path, path);

	ll_t *ll = MUTEX_LOCK(mountpoint_list);
	al = MUTEX_LOCK(g_al);
	int res = ll_add_back(al, ll, mountpoint);
	MUTEX_UNLOCK(g_al);
	MUTEX_UNLOCK(mountpoint_list);

	if (res) {
		al = MUTEX_LOCK(g_al);
		free(al, mountpoint->path);
		free(al, mountpoint);
		MUTEX_UNLOCK(g_al);
		return -1;
	}

	return 0;
}

mountpoint_t *vfs_get_mountpoint(ll_t *ll, const char *path) {
	if (!path)
		return NULL;

	size_t mp_count = ll_size(ll);
	mountpoint_t *best = NULL;
	size_t best_len = 0;

	for (size_t i = 0; i < mp_count; i++) {
		mountpoint_t *mp = ll_get(ll, i);
		// find the mountpoint which has the longest match with path
		// for example mp1->path = "/home/til" mp2->path="/home"; path = "/home/til/file" -> return mp1

		if (mp == NULL || mp->path == NULL)
			continue;

		size_t mp_len = strlen(mp->path);

		int matches =
			(strncmp(path, mp->path, mp_len) == 0) &&
			(path[mp_len] == '\0' || path[mp_len] == '/');

		if (matches && (best == NULL || mp_len > best_len)) {
			best = mp;
			best_len = mp_len;
		}
	}

	return best;
}

int vfs_umount(const char *path) {
	// lock the list the whole function
	// to prevent race conditions
	ll_t *ll = MUTEX_LOCK(mountpoint_list);
	mountpoint_t *mp = vfs_get_mountpoint(ll, path);
	if (!mp) {
		MUTEX_UNLOCK(mountpoint_list);
		return -1;
	}

	allocator *al = MUTEX_LOCK(g_al);
	free(al, mp->path);
	free(al, mp);

	ll_remove_pointer(al, ll, mp);

	MUTEX_UNLOCK(g_al);
	MUTEX_UNLOCK(mountpoint_list);
}

void vfs_init(void) {
	allocator *al = MUTEX_LOCK(g_al);
	mountpoint_list.data = ll_create(al);
	mutex *mut = alloc(al, sizeof(mutex));
	MUTEX_UNLOCK(g_al);

	mountpoint_list.lock = mutex_create(mut);

	// create the root mountpoint
	fs_operations_t empty_ops = {0};
	int ret = vfs_mount(NULL, "/", &empty_ops);
	if (ret) {
		printk(QEMU_SERIAL | FRAMEBUFFER, "Could not create root mountpoint");
		asm volatile("cli; hlt");
	}
}
