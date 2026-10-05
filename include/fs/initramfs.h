#ifndef _INITRAMFS_H
#define _INITRAMFS_H

#include <stdint.h>
#include <fs/vfs.h>

// for now these are the file operations used to
// access the initramfs they can not be used
// to access an arbitrary ustar filesystem
// in ram or on any disk ONLY initramfs

int initramfs_lookup(uint8_t *archive, char *filename, uint8_t **out);

extern fs_operations_t initramfs_operations;

#endif // _INITRAMFS_H
