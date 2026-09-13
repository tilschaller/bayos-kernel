#ifndef _IO_H
#define _IO_H

#include <stdint.h>

// i acutally not only put io functions in here
// but inline assembly functions

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void io_wait(void);

unsigned long save_irqdisable(void);
void irqrestore(unsigned long flags);

void write_cr3(unsigned long long value);

uint64_t read_cr3(void);

#endif // _IO_H
