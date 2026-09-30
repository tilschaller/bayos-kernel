#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <pipe.h>

void keyboard_process_init(void);

extern pipe *g_keyboard_pipe;

#endif // _KEYBOARD_H
