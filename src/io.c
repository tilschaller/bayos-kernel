#include <io.h>

void outb(uint16_t port, uint8_t val) {
	asm volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile ( "inb %w1, %b0"
	               : "=a"(ret)
	               : "Nd"(port)
	               : "memory");
	return ret;
}

void io_wait(void) {
	outb(0x80, 0);
}

unsigned long save_irqdisable(void)
{
    unsigned long flags;
    asm volatile ("pushf\n\tcli\n\tpop %0" : "=r"(flags) : : "memory");
    return flags;
}

void irqrestore(unsigned long flags)
{
    asm ("push %0\n\tpopf" : : "rm"(flags) : "memory","cc");
}


