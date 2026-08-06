#include <keyboard.h>
#include <interrupts.h>
#include <sched.h>
#include <framebuffer.h>
#include <io.h>

int g_keyboard_pipe;

void keyboard_process(void);

extern void _keyboard_handler;

void keyboard_process_init(void) {
	idt_set_descriptor(0x21, (uintptr_t)&_keyboard_handler, 0x8e, 0);
	// add_process((uintptr_t)&keyboard_process);
	g_keyboard_pipe = pipe_create(0x200);

	uint8_t value = inb(0x21) & ~(1 << 1);
	outb(0x21, value);
}

static uint8_t scancode_to_ascii(uint8_t scancode) {
	switch (scancode) {
		// number row
		case 0x02: return '1';
		case 0x03: return '2';
		case 0x04: return '3';
		case 0x05: return '4';
		case 0x06: return '5';
		case 0x07: return '6';
		case 0x08: return '7';
		case 0x09: return '8';
		case 0x0A: return '9';
		case 0x0B: return '0';

		// letters
		case 0x10: return 'q';
		case 0x11: return 'w';
		case 0x12: return 'e';
		case 0x13: return 'r';
		case 0x14: return 't';
		case 0x15: return 'y';
		case 0x16: return 'u';
		case 0x17: return 'i';
		case 0x18: return 'o';
		case 0x19: return 'p';
		case 0x1E: return 'a';
		case 0x1F: return 's';
		case 0x20: return 'd';
		case 0x21: return 'f';
		case 0x22: return 'g';
		case 0x23: return 'h';
		case 0x24: return 'j';
		case 0x25: return 'k';
		case 0x26: return 'l';
		case 0x2C: return 'z';
		case 0x2D: return 'x';
		case 0x2E: return 'c';
		case 0x2F: return 'v';
		case 0x30: return 'b';
		case 0x31: return 'n';
		case 0x32: return 'm';

		case 0x39: return ' ';

		case 0x1c: return '\n';

		default: return 0; // unmapped scancode (includes key releases, modifiers, etc.)
	}
}

void keyboard_handler(uint8_t scancode) {
	uint8_t ascii = scancode_to_ascii(scancode);
	if (ascii != 0)
		pipe_try_write(g_keyboard_pipe, ascii);
}

/*
void keyboard_process(void) {
	for (;;) {
		uint8_t buf[1];
		int n = pipe_read(g_keyboard_pipe, buf, sizeof(buf));

		if (n == -1)
			continue;

		framebuffer *fb = MUTEX_LOCK(g_fb);
		
		for (int i = 0; i < n; i++) {
			fb->putchar(fb, (int)buf[i]);
		}

		MUTEX_UNLOCK(g_fb);
	}
}
*/
