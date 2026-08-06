#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>

//
// we need these 4 functions, since gcc reserves the right to call these at any time,
// even in a freestanding environment
// probably for optimization purposes
//

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);

#endif  // _STRING_H
