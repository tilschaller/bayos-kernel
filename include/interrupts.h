#ifndef _INTERRUPTS_H
#define _INTERRUPTS_H

#include <stdint.h>

void exceptions_init();

void idt_set_descriptor(uint8_t entry, uint64_t isr, uint8_t flags, uint8_t ist);

#endif // _INTERRUPTS_H
