#include <framebuffer.h>
#include <string.h>

// 
// the external sysmbols provided by the font
// this just contains a bitmap of said font
//
extern char _binary_zap_vga16_psf_start;
extern char _binary_zap_vga16_psf_end;

#define PSF_FONT_MAGIC 0x0436
typedef struct {
	uint16_t magic;
	uint8_t font_mode;
	uint8_t char_size;
} PSF1_header;

//
// NOTE: we assume the framebuffer
// has a 32bit rgb mode, which is not
// guaranteed by the limine boot protocol
//

#define LINE_SPACING 2
#define LETTER_SPACING 0
#define BORDER_PADDING 1

#define CHAR_RASTER_HEIGHT 16
#define CHAR_RASTER_WIDTH 8
#define BACKUP_CHAR 32

#define TAB_SIZE 8

static inline uint64_t width(framebuffer *fb) {
	return fb->info.width;
}

static inline uint64_t height(framebuffer *fb) {
	return fb->info.height;
}

static inline void carriage_return(framebuffer *fb) {
	fb->x_pos = BORDER_PADDING;
}

static inline void newline(framebuffer *fb) {
	fb->y_pos += CHAR_RASTER_HEIGHT + LINE_SPACING;
	carriage_return(fb);
}

static inline void clear(framebuffer *fb) {
	uint64_t size = fb->info.bpp / 8 * width(fb) * height(fb);
	fb->x_pos = BORDER_PADDING;
	fb->y_pos = BORDER_PADDING;
	memset(fb->buffer, 0, size);
}

static inline void write_pixel(framebuffer *fb, uint64_t x, uint64_t y, uint32_t color) {
	uint32_t *arr = fb->buffer;
	arr[x + (y * (fb->info.pitch / 4))] = color;
}

static void write_char(framebuffer *fb, unsigned char c, uint32_t color) {
	int bytes_per_glyph = (CHAR_RASTER_WIDTH * CHAR_RASTER_HEIGHT) / 8;

	uint8_t *glyph = (uint8_t*)&_binary_zap_vga16_psf_start +
	                 sizeof(PSF1_header) +
	                 c * bytes_per_glyph;

	for (int y = 0; y < CHAR_RASTER_HEIGHT; y++) {
		uint8_t pixel_row = glyph[y];
		for (int x = 0; x < CHAR_RASTER_WIDTH; x++) {
			if (pixel_row & (1 << (7 - x))) {
				write_pixel(fb, fb->x_pos + x, fb->y_pos + y, color);
			}
		}
	}

	fb->x_pos += CHAR_RASTER_WIDTH + LETTER_SPACING;
}

static int32_t putchar(framebuffer *fb, int32_t c) {
	switch (c) {
	case '\n':
		newline(fb);
		break;
	case '\r':
		carriage_return(fb);
		break;
	default:
		uint64_t new_xpos = fb->x_pos + CHAR_RASTER_WIDTH;
		if (new_xpos >= width(fb)) {
			newline(fb);
		}
		uint64_t new_ypos = fb->y_pos + CHAR_RASTER_HEIGHT + BORDER_PADDING;
		if (new_ypos >= height(fb)) {
			clear(fb);
		}
		write_char(fb, (char)c, 0xffffffff);
	}
	
	return 0;
}

void framebuffer_init(struct limine_framebuffer *info, framebuffer *fb) {
	fb->info.width = info->width;
	fb->info.height = info->height;
	fb->info.bpp = info->bpp;
	fb->info.pitch = info->pitch;
	fb->buffer = info->address;
	fb->putchar = &putchar;
	clear(fb);
}


int g_framebuffer_print_pipe;

void framebuffer_print_process(void) {
	for (;;) {
		unsigned char buf;
		pipe_read(g_framebuffer_print_pipe, &buf, sizeof(buf));

        	framebuffer *fb = MUTEX_LOCK(g_fb);
        	fb->putchar(fb, (int)buf);
        	MUTEX_UNLOCK(g_fb);
	}
}
