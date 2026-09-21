#include <fs/ustar.h>
#include <string.h>

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

int tar_lookup(uint8_t *archive, char *filename, uint8_t **out)
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
