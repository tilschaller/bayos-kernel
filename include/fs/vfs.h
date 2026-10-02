#ifndef _VFS_H
#define _VFS_H

#include <stdint.h>

typedef struct file_t {} file_t;

typedef struct fs_operations_t {
	file_t *(*open)(const char *path, int flags);
	size_t (*read)(file_t *file, void *buf, size_t count);
	size_t (*write)(file_t *file, void *buf, size_t count);
	int (*close)(file_t *file);
} fs_operations_t;

typedef struct device_t {} device_t;

typedef struct mountpoint_t {
	device_t *device;

	char *path;

	fs_operations_t *operations;
} mountpoint_t;

void vfs_init(void);

#endif // _VFS_H
