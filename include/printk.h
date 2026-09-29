#ifndef _PRINTK_H
#define _PRINTK_H

#include <stdarg.h>

typedef enum {
	QEMU_SERIAL = 1,
	FRAMEBUFFER = 2,
	// ... = 4
} printk_output_type;

int printk(printk_output_type type, const char *fmt, ...);

#endif // _PRINTK_H