#ifndef _SCHED_H
#define _SCHED_H

#include <stdint.h>

struct allocator;
void sched_init(struct allocator *al);

typedef enum {
	EMPTY = 0,
	PIPE,
	FILE,
} resource_type;

typedef struct {
	resource_type type;
	void *res;
} resource;

#define RESOURCES_INITIAL_CAP 128
void resource_table_free(resource *t);
resource *resource_table_alloc(size_t cap);

typedef enum {
	READY,
	RUNNING,
	BLOCKED,
	DEAD,
} process_status;

//
// this is the cpu_status passed to the schedule function
// over the stack
//
typedef struct {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;

	struct {
		uint64_t rip;
		uint64_t cs;
		uint64_t flags;
		uint64_t rsp;
		uint64_t ss;
	} iret;
} cpu_status;

typedef struct process {
	uint64_t cr3;
	process_status status;
	cpu_status *context;

	struct process *next;
	struct process *wait_next;

	int pid;

	// these following structures depend on the process being a kernel or user process
	// maybe we could split this struct into 2 smaller ones?
	//
	// kernel things
	uint8_t *kernel_stack;

	// user things
	resource *resources;
	int resources_cap;
	uint64_t anon_allocate_end;
	uint32_t elf_end;
} process;

void yield(void);

void insert_process(process *p);
void add_process(uintptr_t func);
void mark_current_proc_as_dead(void);

process *get_current_process();

// add a resource to the process struct
// returns the file descriptor
int resource_add(resource_type type, void *type_id);
// remove it from the process struct
// this returns the type_id
// you need to deallocate it yourself
// if no resources is there, returns -1
void *resource_remove(int fd);

// read from a resource
int resource_read(int fd, uint8_t *buf, size_t len);
// write to a resource
int resource_write(int fd, const uint8_t *buf, size_t len);

int get_new_pid(void);
process *get_proc_from_pid(int);
void remove_proc_from_list(process *p);

#endif // _SCHED_H
