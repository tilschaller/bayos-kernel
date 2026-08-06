LD = ld
AS = nasm
CC = gcc

CFLAGS = \
	-m64 \
	-Iinclude \
	-nostdlib \
	-Wall -Wextra \
	-fno-pie \
	-fno-builtin \
	-fno-exceptions \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables \
	-fno-ident \
	-fno-stack-protector \
	-mno-red-zone \
	-mcmodel=kernel \
	-MMD \
	-mgeneral-regs-only \
	-O2 \
	-nostdinc \
	-ffreestanding \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-ffunction-sections \
	-fdata-sections

ASM_SRCS = \
	src/exception_handler.asm \
	src/interrupt_handler.asm

C_SRCS = \
	 src/main.c \
	 src/string.c \
	 src/framebuffer.c \
	 src/early.c \
	 src/interrupts.c \
	 src/gdt.c \
	 src/memory.c \
	 src/alloc.c \
	 src/sched.c \
	 src/keyboard.c \
	 src/io.c \
	 src/syscall.c \
	 src/fs/ustar.c \
	 src/elf.c

ASM_OBJS = $(patsubst %.asm,%.o,$(ASM_SRCS))
C_OBJS = $(patsubst %.c,%.o,$(C_SRCS))
OBJS = $(ASM_OBJS) $(C_OBJS)

-include $(C_OBJS:.o=.d)

kernel: $(OBJS) kernel.ld Makefile assets/font.o
	$(LD) -o $@ $(OBJS) assets/font.o -T kernel.ld

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm Makefile
	$(AS) -f elf64 $< -o $@

.PHONY: clean
clean: 
	rm -f kernel $(OBJS) $(C_OBJS:.o=.d)

DESTDIR ?=
PREFIX ?= /boot

.PHONY: install
install: kernel
	install -d "$(DESTDIR)$(PREFIX)"
	install -m 644 kernel "$(DESTDIR)$(PREFIX)/kernel.elf"
