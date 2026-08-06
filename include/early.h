#ifndef _EARLY_H
#define _EARLY_H

// 
// functions in here should only be used in the context
// of early boot, that means no scheduler is running,
// interrupts are disabled.
//

#include <framebuffer.h>

//
// this is basically puts for now
//
void early_printk_init(framebuffer *fb);
void early_printk(char *str);

#endif // _EARLY_H
