#include <early.h>

//
// see disclaimer at top of early.h
// for usage concerns of these functions
//

//
// this should only be initialized once in early boot
// and used only there
//
framebuffer *fb_default = NULL;

void early_printk_init(framebuffer *fb) {
	fb_default = fb;
}
void early_printk(char *str) {
	if (!fb_default) return;

	char *arr = str;
	while (*arr != '\0') {
		fb_default->putchar(fb_default, (int)*arr);

		arr++;
	}
}

