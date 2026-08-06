#ifndef _USTAR_H
#define _USTAR_H

#include <stdint.h>

int tar_lookup(uint8_t *archive, char *filename, uint8_t **out);

#endif // _USTAR_H
