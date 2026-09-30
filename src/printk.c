#include <printk.h>
#include <framebuffer.h>
#include <pipe.h>
#include <io.h>

// print a char to the qemu serial
static void putchar_qemu_serial(int c)
{
	outb(0xe9, (uint8_t)c);
}

// print a char to the framebuffer
static void putchar_framebuffer(int c)
{
	pipe_write(g_framebuffer_print_pipe, (uint8_t *)&c, 1);
}

framebuffer *fb_default = NULL;

void early_printk_init(framebuffer *fb)
{
	fb_default = fb;
}

static void putchar_early(int c)
{
	if (!fb_default) return;
	fb_default->putchar(fb_default, c);
}

static void putchar(printk_output_type type, int c)
{
	if (type & QEMU_SERIAL) {
		putchar_qemu_serial(c);
	}

	if (type & FRAMEBUFFER) {
		putchar_framebuffer(c);
	}

	if (type & EARLY) {
		putchar_early(c);
	}
}

static int format_spec_x(printk_output_type type, uint64_t val)
{
	char digits[] = "0123456789abcdef";
	char buffer[sizeof(val) * 2];
	int i = 0;
	int count = 0;

	if (val == 0) {
		putchar(type, (int)'0');
		return 1;
	}

	while (val > 0) {
		buffer[i++] = digits[val % 16];
		val /= 16;
	}

	while (i > 0) {
		putchar(type, buffer[--i]);
		count++;
	}

	return count;
}

static int format_spec_s(printk_output_type type, uint8_t *str)
{
	int count = 0;

	char *c = str;
	while (*c) {
		putchar(type, (int)*c);

		count++;
		c++;
	}

	return count;
}

static int format_spec_d(printk_output_type type, int val)
{
	int number = val;
	int count = 0;

	if (number < 0) {
		count++;
		putchar(type, (int)'-');
		number = -number;
	}

	if (number >= 10)
		count += format_spec_d(type, number / 10);

	putchar(type, (int)('0' + number % 10));
	count++;
	return count;
}

int printk(printk_output_type type, const char *fmt, ...)
{
	if (!fmt)
		return 0;

	int count = 0;

	va_list args;
	va_start(args, fmt);

	char *c = fmt;
	while (*c) {
		if (*c == '%') {
			c++;

			switch (*c) {
				case 'x':
					count += format_spec_x(type, va_arg(args, uint64_t));
					break;
				case '%':
					putchar(type, (int)*c);
					count++;
					break;
				case 'c':
					putchar(type, va_arg(args, int));
					count++;
					break;
				case 's':
					count += format_spec_s(type, va_arg(args, uint8_t *));
					break;
				case 'd':
					count += format_spec_d(type, va_arg(args, int));
					break;
			}
		}
		else {
			putchar(type, (int)*c);
			count++;
		}
		c++;
	}

	va_end(args);
	return count;
}
