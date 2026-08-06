#include <interrupts.h>
#include <early.h>

// 
// this is the general exception handler
// for interrupts 0-31, called by the functions in 
// exception_handler.asm
// NOTE: these functions do not return, but print a error instead
// also NOTE: these use early_prinkt for now, which is not really proper, 
// but the best solution for now. since it doesnt really matter, if the framebuffer
// gets corrupted. we should just try to output something useful before halting completely
//
// num is the interrupt number
// err is the error code, if there is one
//
__attribute__((noreturn))
void exception_handler(uint64_t num, uint64_t err) {
	(void)num; (void)err;
	early_printk("exception occured\n");
	early_printk("halting execution\n");

	// we completely halt down the processor
	asm volatile("cli");
	asm volatile("hlt");

	// never actually reached, but supresses compiler warnings
	for (;;);
}

//
// one entry in the idt table
//
typedef struct {
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t ist;
	uint8_t attr;
	uint16_t isr_mid;
	uint32_t isr_high;
	uint32_t reserved;
} __attribute__((packed)) idt_entry;

//
// this is the interrupts table,
// we have 256 entries
// the first 32 are reserved for
// exceptions: https://wiki.osdev.org/Exceptions
//
__attribute__((aligned(0x10)))
static idt_entry idt[0x100];

// 
// this is the idt register structure
// you just write the size and porinter
// to the idt table in it and can load it
//
typedef struct {
	uint16_t size;
	idt_entry *idt;
} __attribute__((packed)) idt_register;

//
// this function writes a function pointer into the idt table
// and sets required flags
//
void idt_set_descriptor(uint8_t entry, uint64_t isr, uint8_t flags, uint8_t ist) {
	idt[entry].isr_low = isr & 0xffff;
	idt[entry].kernel_cs = 0x8;
	idt[entry].ist = ist & 0x7;
	idt[entry].attr = flags;
	idt[entry].isr_mid = (isr >> 16) & 0xffff;
	idt[entry].isr_high = (isr >> 32) & 0xffffffff;
	idt[entry].reserved = 0;
}

//
// these are the handlers for the exceptions defined in exception_handler.asm
//
extern uintptr_t isr_stub_table[32];

// 
// this function is called from the _start function
// it sets up the interrupt table
// and writes the exception handlers in it
//
void exceptions_init() {
	// create a valid idtr
	idt_register idtr = {
		sizeof(idt) - 1,
		idt,
	};

	// set the first 31 entries of the interrupt table
	// these are set to the exception handlers
	for (int i = 0; i < 32; i++) {
		if (i == 0x8) {
			// the double fault gets special treatment
			// we set the ist to one, so the stack gets switched when this occurs
			// so we can guarantee the cpu never crashes, but instead always ends up here
			idt_set_descriptor(i, isr_stub_table[i], 0x8e, 1);
		} else {
			// this just uses the stack it was on when the exception occured
			idt_set_descriptor(i, isr_stub_table[i], 0x8e, 0);

		}
	}

	// last we actually load it
	asm volatile ("lidt %0" : : "m"(idtr) : "memory");
}
