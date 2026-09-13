#ifndef _SCHED_H
#define _SCHED_H

#include <stdint.h>

struct allocator;
void sched_init(struct allocator *al);

typedef enum {
	EMPTY = 0,
	PIPE,
} resource_type;

typedef struct {
	resource_type type;
	int type_id;
} resource;

#define MAX_RESOURCES 0x100

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
	resource resources[MAX_RESOURCES];
	uint64_t anon_allocate_end;
	uint32_t elf_end;
} process;

void insert_process(process *p);
void add_process(uintptr_t func);
void mark_current_proc_as_dead(void);

int sem_create(int initial_count);
void sem_wait(int sem_id);
int sem_trywait(int sem_id);
void sem_signal(int sem_id);
void sem_destroy(int sem_id);

#define mutex_create()		sem_create(1)
#define mutex_lock(m)		sem_wait(m)
#define mutex_try_lock(m)	sem_trywait(m)
#define mutex_unlock(m)		sem_signal(m)
#define mutex_destroy(m)	sem_destroy(m)

//
// these are the mutex function you are most likely to want to use
//
// these defines a mutex type
#define DEFINE_MUTEX_TYPE(T) \
	typedef struct { \
		T *data; \
		int lock; \
	} Mutex_##T

// this is how you would refer to a mutex type
#define Mutex(T) Mutex_##T

#define MUTEX_LOCK(m)	( mutex_lock((m).lock), (m).data )
#define MUTEX_TRY_LOCK(m) ( mutex_try_lock((m).lock) ? (m).data : NULL )
#define MUTEX_UNLOCK(m) mutex_unlock((m).lock)

int pipe_create(size_t buffer_len);
int pipe_write(int pipe_id, const uint8_t *data, size_t len);
int pipe_try_write(int pipe_id, uint8_t byte);
int pipe_read(int pipe_id, uint8_t *out, size_t len);
void pipe_close(int pipe_id);

process *get_current_process();

// add a resource to the process struct
// returns the file descriptor
int resource_add(resource_type type, int type_id);
// remove it from the process struct
// this returns the type_id
// you need to deallocate it yourself
// if no resources is there, returns -1
int resource_remove(int fd);

// read from a resource
int resource_read(int fd, uint8_t *buf, size_t len);
// write to a resource
int resource_write(int fd, const uint8_t *buf, size_t len);

#endif // _SCHED_H
