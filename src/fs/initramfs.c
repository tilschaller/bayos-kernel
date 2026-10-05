#include <fs/initramfs.h>
#include <string.h>
#include <alloc.h>
#include <string.h>
#include <limine.h>

extern volatile struct limine_module_request module_request;
#define INITRAMFS ((uint8_t*)module_request.response->modules[0]->address)

static int oct2bin(uint8_t *str, int size)
{
	int n = 0;
	uint8_t *c = str;

	while (size-- > 0) {
		n *= 8;
		n += *c - '0';
		c++;
	}
	return n;
}

int initramfs_lookup(uint8_t *archive, char *filename, uint8_t **out)
{
	uint8_t *ptr = archive;
	if (filename == NULL)
		return -1;

	while (!memcmp(ptr + 257, "ustar", 5)) {
		int filesize = oct2bin(ptr + 0x7c, 11);
		if (!memcmp(ptr + 1, filename, strlen(filename) + 1)) {
			*out = ptr + 512;
			return filesize;
		}
		ptr += (((filesize + 511) / 512) + 1) * 512;
	}
	return 0;
}

static file_t *open(const char *path, int flags) {
	// does the file exist?
	uint8_t *out;
	if (!initramfs_lookup(INITRAMFS, path, &out))
		return NULL;

	allocator *al = MUTEX_LOCK(g_al);
	file_t *file = alloc(al, sizeof(file_t));
	MUTEX_UNLOCK(g_al);

	if (!file)
		return NULL;

	al = MUTEX_LOCK(g_al);
	file->path = alloc(al, strlen(path) + 1);
	MUTEX_UNLOCK(g_al);

	if (!file->path) {
		al = MUTEX_LOCK(g_al);
		free(al, file);
		MUTEX_UNLOCK(g_al);
		return NULL;
	}

	strcpy(file->path, path);

	file->offset = 0;

	return file;
}

// the filesystem stores the whole path of the file including the mountpoint prefix
// in file->path. this shouldnt matter, since the initramfs always gets mounted at '/'
// i dont really wanna do this properly right now , so im doing it like this

static size_t read(file_t *file, void *buf, size_t count) {
	uint8_t *file_buf;
	int filesz = initramfs_lookup(INITRAMFS, file->path, &file_buf);
	// the file is guaranteed to exist
	// we already checked when creating it
	
	// copy the contents of the file of the file
	if (file->offset >= filesz)
		return 0;

	size_t remaining = filesz - file->offset;
	size_t to_copy = count < remaining ? count : remaining;

	memcpy(buf, file_buf + file->offset, to_copy);
	file->offset += to_copy;

	return to_copy;
}

static size_t write(file_t *file, void *buf, size_t count) {
	return 0;
	// cant write to initramfs
}

static int close(file_t *file) {
	// deallocate the file
	allocator *al = MUTEX_LOCK(g_al);
	free(al, file->path);
	free(al, file);
	MUTEX_UNLOCK(g_al);;
}

fs_operations_t initramfs_operations = {
	.open = open,
	.read = read,
	.write = write,
	.close = close,
};
