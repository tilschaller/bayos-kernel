#include <sched.h>
#include <alloc.h>
#include <stdint.h>
#include <memory.h>
#include <interrupts.h>
#include <string.h>
#include <io.h>
#include <printk.h>
#include <pipe.h>

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43


process *process_list = NULL;
process *current_process = NULL;

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

extern void timer_int_handler;


void sched_init(struct allocator *al)
{
	//
	// first we need to set up a timer interrupt, so
	// we can even create a preemptive scheduler
	//
	// i dont wanna bother with acpi right now, so we are just going to use
	// the legacy pit. it is still present on almost all new systems
	//
	// the timer should trigger an interupt every 10ms
	//
	// but first we need to install an intterupt handler
	// and remap the pic
	//
	idt_set_descriptor(0x20, (uintptr_t)&timer_int_handler, 0x8e, 0);

	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
	io_wait();

	// we configure the master pic to use interrupts 0x44 - 0x4b
	// the slave pic uses 0x4c - (0x4b + 8)
	outb(PIC1_DATA, 0x20);
	io_wait();
	outb(PIC2_DATA, 0x28);
	io_wait();

	outb(PIC1_DATA, 0x04);
	io_wait();
	outb(PIC2_DATA, 0x02);
	io_wait();

	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	outb(PIC1_DATA, ~1);
	io_wait();
	outb(PIC2_DATA, 0xff);
	io_wait();

	//
	// now we configure the timer to fire every 10ms
	//
	uint16_t divisor = (uint16_t)(1193182 / 100);

	outb(PIT_COMMAND, 0x36);
	outb(PIT_CHANNEL0, divisor & 0xff);
	outb(PIT_CHANNEL0, (divisor >> 8) & 0xff);

	// now as soon as we enable interrupts, one will fire every 10ms

	// so that our schedule function works, we need to create a initial process,
	// (our) _start function
	process *p = alloc(al, sizeof(process));
	memset(p, 0, sizeof(process));
	p->status = RUNNING;
	p->pid = -1;
	p->cr3 = read_cr3();
	process_list = p;
	current_process = p;
}

//
// this function will schedule a process
// NOTE: this runs with interrupts disabled
//
cpu_status *schedule(cpu_status *context)
{
	current_process->context = context;
	if (current_process->status != DEAD && current_process->status != BLOCKED)
		current_process->status = READY;

	for (;;) {
		process *prev_process = current_process;
		if (current_process->next != NULL) {
			current_process = current_process->next;
		}
		else {
			current_process = process_list;
		}

		if (current_process != NULL &&
		    (current_process->status == DEAD || current_process->status == BLOCKED)) {
			// do nothing, kernel processes should run forever or take care of theyre own cleanup
			// user processes should be reaped by wait eventually
		}
		else {
			current_process->status = RUNNING;
			break;
		}
	}

	return current_process->context;
}

void add_process(uintptr_t func)
{
	// create a new process struct
	allocator *al = MUTEX_LOCK(g_al);

	process *p = alloc(al, sizeof(process));
	uint8_t *stack = alloc(al, 0x2000);
	cpu_status *context = (cpu_status *)(stack + 0x2000 - sizeof(cpu_status));

	MUTEX_UNLOCK(g_al);

	memset(p, 0, sizeof(process));
	memset(stack, 0, 0x2000);

	p->status = READY;
	p->pid = 0;
	// keep the base pointer so the full stack allocation can be freed later
	p->kernel_stack = stack;

	memset(context, 0, sizeof(*context));

	context->iret.rip = func;
	context->iret.cs = 0x8;
	context->iret.flags = 0x202;
	context->iret.rsp = (uintptr_t)stack + 0x2000 - 0x10;
	context->iret.ss = 0x10;

	p->context = context;
	p->anon_allocate_end = 0x800000;
	p->elf_end = 0;
	p->cr3 = read_cr3();
	unsigned long save = save_irqdisable();
	asm volatile("mov %%cr3, %0" : "=r"(p->cr3));
	irqrestore(save);

	insert_process(p);
}

void mark_current_proc_as_dead(void)
{
	unsigned long flags = save_irqdisable();

	get_current_process()->status = DEAD;

	irqrestore(flags);

	yield();
}



void insert_process(process *p)
{
	unsigned long flags = save_irqdisable();

	process *last = process_list;
	while (last->next != 0)
		last = last->next;

	last->next = p;

	irqrestore(flags);
}

// manually trigger the timer interrupt
void yield(void)
{
	asm volatile("int $0x20");
}

int resource_add(resource_type type, void *res)
{
	unsigned long flags = save_irqdisable();

	for (int i = 0; i < MAX_RESOURCES; i++) {
		if (current_process->resources[i].type == EMPTY) {
			current_process->resources[i].type = type;
			current_process->resources[i].res = res;

			irqrestore(flags);
			return i;
		}
	}

	irqrestore(flags);
	return -1;

}
void *resource_remove(int fd)
{
	unsigned long flags = save_irqdisable();

	void *ret = NULL;
	if (current_process->resources[fd].type != EMPTY)
		ret = current_process->resources[fd].res;

	current_process->resources[fd].type = EMPTY;

	irqrestore(flags);
	return ret;
}

process *get_current_process()
{
	unsigned long flags = save_irqdisable();

	process *proc = current_process;

	irqrestore(flags);

	return proc;
}

int resource_read(int fd, uint8_t *buf, size_t len)
{
	process *proc = get_current_process();

	switch (proc->resources[fd].type) {
		case PIPE:
			return pipe_read((pipe*)proc->resources[fd].res, buf, len);
		default:
			return -1;
	}
}

int resource_write(int fd, const uint8_t *buf, size_t len)
{
	process *proc = get_current_process();

	switch (proc->resources[fd].type) {
		case PIPE:
			return pipe_write((pipe*)proc->resources[fd].res, buf, len);
		default:
			return -1;
	}
}

static uint32_t next_pid = 0;
int get_new_pid(void) {
	int pid = (int)__atomic_fetch_add(
        &next_pid,
        1,
        __ATOMIC_SEQ_CST
    );

    printk(QEMU_SERIAL, "New Pid generated: %d\n", pid);

    return pid;
}

process *get_proc_from_pid(int pid) {
	unsigned long flags = save_irqdisable();

	process *curr = process_list;
	while (curr != NULL) {
		if (curr->pid == pid) {
			irqrestore(flags);
			return curr;
		}

		curr = curr->next;
	}

	irqrestore(flags);
	return NULL;
}

void remove_proc_from_list(process *p)
{
    if (p == nullptr || process_list == nullptr)
        return;

    if (process_list == p) {
        process_list = process_list->next;
        p->next = nullptr;
        return;
    }

    process *current = process_list;

    while (current->next != nullptr) {
        if (current->next == p) {
            current->next = p->next;
            p->next = nullptr;
            return;
        }

        current = current->next;
    }
}
