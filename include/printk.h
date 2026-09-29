#ifndef _PRINTK_H
#define _PRINTK_H

#include <stdarg.h>
#include <framebuffer.h>

typedef enum {
	// serial can always be used
	QEMU_SERIAL = 1,
	// framebuffer target can only be used after the scheduler 
	// was started and the framebuffer print process was added
	FRAMEBUFFER = 2,
	// this is for early boot, before the scheduler was started
	// and exceptions
	EARLY = 4,
} printk_output_type;

int printk(printk_output_type type, const char *fmt, ...);

void early_printk_init(framebuffer *fb);

#endif // _PRINTK_H