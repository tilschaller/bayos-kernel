#include <sched.h>
#include <alloc.h>
#include <stdint.h>
#include <memory.h>
#include <interrupts.h>
#include <string.h>
#include <io.h>
#include <early.h>

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


void sched_init(struct allocator *al) {
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
	process_list = p;
	current_process = p;
}


//
// this function is called from the schedule function only
// if a function has been marked as dead
// it should free all objects associated with the function and
// free all memory
// NOTE: this runs with interrupts disabled, since its only 
// called from the schedule function
// HINT: if you want to delete a process manually do it with
// mark_current_proc_as_dead()
//
// if we cant get the allocator we just skip this schedule round and let it be
// till the allocator gets free sometimes
//
static void delete_process_resources(void) {
	allocator *al = MUTEX_TRY_LOCK(g_al);
	if (!al) return;
	
	if (current_process->kernel_stack)
		free(al, current_process->kernel_stack);
	free(al, current_process);

	MUTEX_UNLOCK(g_al);
}

//
// this function will schedule a process
// NOTE: this runs with interrupts disabled
//
cpu_status *schedule(cpu_status *context) {
	current_process->context = context;
	if (current_process->status != DEAD && current_process->status != BLOCKED)
		current_process->status = READY;

	for (;;) {
		process *prev_process = current_process;
		if (current_process->next != NULL) {
			current_process = current_process->next;
		} else {
			current_process = process_list;
		}

		if (current_process != NULL && 
		    (current_process->status == DEAD || current_process->status == BLOCKED)) {
			if (current_process->status == DEAD) {
				prev_process->next = current_process->next;
				delete_process_resources();
				current_process = prev_process;
			}
		} else {
			current_process->status = RUNNING;
			break;
		}
	}

	return current_process->context;
}

void add_process(uintptr_t func) {
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

	unsigned long flags = save_irqdisable();

	process *last = process_list;
	while (last->next != 0) 
		last = last->next;

	last->next = p;

	irqrestore(flags);
}


//
// we need some lock, which is dependent on the scheduler
// i am gonna be implementing semaphores
//
// maybe make this dynamic sometimes
// alloc is also behind a semaphore, so kinda hard i think
#define MAX_SEMAPHORES 128

typedef struct semaphore {
	int in_use;
	int count;
	process *wait_head;
	process *wait_tail;
} semaphore;

static semaphore sem_table[MAX_SEMAPHORES] = {0};

int sem_create(int initial_count) {
	unsigned long flags = save_irqdisable();

	for (int i = 0; i < MAX_SEMAPHORES; i++) {
		if (!sem_table[i].in_use) {
			sem_table[i].in_use = 1;
			sem_table[i].count = initial_count;
			sem_table[i].wait_head = NULL;
			sem_table[i].wait_tail = NULL;
			asm volatile("sti");
			return i;
		}
	}
	irqrestore(flags);
	return -1;
}

// manually trigger the timer interrupt
static inline void yield(void) {
	asm volatile("int $0x20");
}

// TODO: we should probably also delete the resources used by this process here
// like for example the open files and allocated pages
// otherwise we have huge memory leaks
void mark_current_proc_as_dead(void) {
	unsigned long flags = save_irqdisable();

	current_process->status = DEAD;
	
	irqrestore(flags);

	yield();
}

void sem_wait(int sem_id) {
	unsigned long flags = save_irqdisable();

	semaphore *sem = &sem_table[sem_id];
	sem->count--;

	if (sem->count < 0) {
		current_process->status = BLOCKED;
		current_process->wait_next = NULL;

		if (sem->wait_tail) {
			sem->wait_tail->wait_next = current_process;
		} else {
			sem->wait_head = current_process;
		}
		sem->wait_tail = current_process;
		irqrestore(flags);
		yield();
		return;
	}

	irqrestore(flags);
}


// returns 1 if acquired without blocking, 0 if would have blocked
int sem_trywait(int sem_id) {
	unsigned long flags = save_irqdisable();

	semaphore *sem = &sem_table[sem_id];
	if (sem->count > 0) {
		sem->count--;
		irqrestore(flags);
		return 1;
	}

	irqrestore(flags);
	return 0;
}

void sem_signal(int sem_id) {
	unsigned long flags = save_irqdisable();

	semaphore *sem = &sem_table[sem_id];
	sem->count++;

	if (sem->count <= 0 && sem->wait_head != NULL) {
		process *woken = sem->wait_head;
		sem->wait_head = woken->wait_next;
		if (sem->wait_head == NULL) sem->wait_tail = NULL;
		woken->wait_next = NULL;
		woken->status = READY;
	}

	irqrestore(flags);
}

void sem_destroy(int sem_id) {
	unsigned long flags = save_irqdisable();

	sem_table[sem_id].in_use = 0;

	irqrestore(flags);
}

//
// this are functions for pipes
// the number of pipes is also limited for now
// this should be changed
//
#define MAX_PIPES 32

typedef struct {
	uint8_t *buffer;
	size_t buffer_len;

	size_t read_pos;
	size_t write_pos;
	int in_use;

	int read_refs;
	int write_refs;

	int buf_lock;
	int sem_empty;
	int sem_full;
} pipe;

static pipe pipe_table[MAX_PIPES];

int pipe_create(size_t buffer_len) {
	unsigned long flags = save_irqdisable();

	for (int i = 0; i < MAX_PIPES; i++) {
		if (!pipe_table[i].in_use) {
			pipe *p = &pipe_table[i];
			
			allocator *al = MUTEX_LOCK(g_al);
			p->buffer = alloc(al, buffer_len);
			MUTEX_UNLOCK(g_al);

			p->buffer_len = buffer_len;
			p->in_use = 1;
			p->read_pos = 0;
			p->write_pos = 0;
			p->read_refs = 1;
			p->write_refs = 1;
			p->buf_lock = mutex_create();
			p->sem_empty = sem_create(buffer_len);
			p->sem_full = sem_create(0);

			irqrestore(flags);
			return i;
		}
	}

	irqrestore(flags);
	return -1;
}

int pipe_write(int pipe_id, const uint8_t *data, size_t len) {
	pipe *p = &pipe_table[pipe_id];

	if (!p->in_use || p->read_refs == 0)
		return -1;

	for (size_t i = 0; i < len; i++) {
		sem_wait(p->sem_empty);

		if (p->read_refs == 0) {
			return i;
		}

		mutex_lock(p->buf_lock);
		p->buffer[p->write_pos] = data[i];
		p->write_pos = (p->write_pos + 1) % p->buffer_len;
		mutex_unlock(p->buf_lock);

		sem_signal(p->sem_full);
	}

	return len;
}

int pipe_try_write(int pipe_id, uint8_t byte) {
	pipe *p = &pipe_table[pipe_id];

	if (!p->in_use || p->read_refs == 0)
		return -1;

	if (!sem_trywait(p->sem_empty)) {
		return 0;
	}

	unsigned long flags = save_irqdisable();

	// TODO: check if it is a problem
	// the buffer mutex lock is not acquired

	p->buffer[p->write_pos] = byte;
	p->write_pos = (p->write_pos + 1) % p->buffer_len;

	irqrestore(flags);

	sem_signal(p->sem_full);
	return 1;
}

int pipe_read(int pipe_id, uint8_t *out, size_t len) {
	pipe *p = &pipe_table[pipe_id];

	if (!p->in_use) return -1;

	size_t i = 0;

	if (len == 0) return 0;

	if (p->write_refs == 0 && sem_table[p->sem_full].count == 0)
		return 0;

	sem_wait(p->sem_full);
	mutex_lock(p->buf_lock);
	out[i] = p->buffer[p->read_pos];
	p->read_pos = (p->read_pos + 1) % p->buffer_len;
	mutex_unlock(p->buf_lock);
	sem_signal(p->sem_empty);
	i++;

	for (; i < len; i++) {
		if (!sem_trywait(p->sem_full))
			break; // nothing more ready right now
		mutex_lock(p->buf_lock);
		out[i] = p->buffer[p->read_pos];
		p->read_pos = (p->read_pos + 1) % p->buffer_len;
		mutex_unlock(p->buf_lock);
		sem_signal(p->sem_empty);
	}

	return i;

	return i;
}

void pipe_close_write(int pipe_id) {
	pipe *p = &pipe_table[pipe_id];
	mutex_lock(p->buf_lock);
	p->write_refs--;
	if (p->write_refs == 0)
		sem_signal(p->sem_full);
	mutex_unlock(p->buf_lock);
}

void pipe_close_read(int pipe_id) {
	pipe *p = &pipe_table[pipe_id];
	mutex_lock(p->buf_lock);
	p->read_refs--;
	if (p->read_refs == 0)
		sem_signal(p->sem_empty);
	mutex_unlock(p->buf_lock);
}

void pipe_close(int pipe_id) {
	pipe_table[pipe_id].in_use = 0;
}

int resource_add(resource_type type, int type_id) {
	unsigned long flags = save_irqdisable();

	for (int i = 0; i < MAX_RESOURCES; i++) {
		if (current_process->resources[i].type == EMPTY) {
			current_process->resources[i].type = type;
			current_process->resources[i].type_id = type_id;

			irqrestore(flags);
			return i;
		}
	}

	irqrestore(flags);
	return -1;

}
int resource_remove(int fd) {
	unsigned long flags = save_irqdisable();

	int ret = -1;
	if (current_process->resources[fd].type != EMPTY)
		ret = current_process->resources[fd].type_id;

	current_process->resources[fd].type = EMPTY;

	irqrestore(flags);
	return ret;
}

process *get_current_process() {
	unsigned long flags = save_irqdisable();

	process *proc = current_process;

	irqrestore(flags);

	return proc;
}

int resource_read(int fd, uint8_t *buf, size_t len) {
	process *proc = get_current_process();

	switch (proc->resources[fd].type) {
		case PIPE:
			return pipe_read(proc->resources[fd].type_id, buf, len);
		default:
			return -1;
	}
}

int resource_write(int fd, const uint8_t *buf, size_t len) {
	process *proc = get_current_process();

	switch (proc->resources[fd].type) {
		case PIPE:
			return pipe_write(proc->resources[fd].type_id, buf, len);
		default:
			return -1;
	}
}
