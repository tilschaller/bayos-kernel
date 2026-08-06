#ifndef _IO_H
#define _IO_H

#include <stdint.h>

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void io_wait(void);

unsigned long save_irqdisable(void);
void irqrestore(unsigned long flags);

#endif // _IO_H
