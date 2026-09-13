#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <stdint.h>
#include <limine.h>
#include <sched.h>

extern int g_framebuffer_print_pipe;
void framebuffer_print_process(void);

//
// this struct contains one instance of a framebuffer
//
typedef struct framebuffer {
	struct {
		uint64_t width;
		uint64_t height;
		uint64_t bpp;
		uint64_t pitch;
	} info;
	uint64_t x_pos;
	uint64_t y_pos;
	void *buffer;
	int (*putchar)(struct framebuffer *, int);

} framebuffer;

//
// this function takes a pointer to a limine framebuffer as received by the bootloader
// and writes a framebuffer to fb
// this can be used to write characters to it
//
void framebuffer_init(struct limine_framebuffer *info, framebuffer *fb);

//
// this is the global instance defined in main.c
//
DEFINE_MUTEX_TYPE(framebuffer);
extern Mutex(framebuffer) g_fb;

#endif // _FRAMEBUFFER_H
