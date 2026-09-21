#include <syscall.h>
#include <alloc.h>
#include <string.h>
#include <stdint.h>
#include <framebuffer.h>
#include <sched.h>
#include <keyboard.h>
#include <memory.h>
#include <io.h>
#include <early.h>
#include <elf.h>
#include <fs/ustar.h>

extern void _syscall_handler;

static inline uint64_t rdmsr(uint32_t msr)
{
	uint32_t lo;
	uint32_t hi;

	asm volatile(
	        "rdmsr"
	        : "=a"(lo), "=d"(hi)
	        : "c"(msr)
	);

	return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
	uint32_t lo = (uint32_t)value;
	uint32_t hi = (uint32_t)(value >> 32);

	asm volatile(
	        "wrmsr"
	        :
	        : "c"(msr), "a"(lo), "d"(hi)
	        : "memory"
	);
}

void syscall_init(void)
{
	// enable syscalls
	// also some things needed for userspace programs are
	// Set here, like enabling sse and setting swags
	uint64_t efer = rdmsr(0xC0000080);
	wrmsr(0xC0000080, efer | 1);

	wrmsr(0xC0000082, (uintptr_t)&_syscall_handler);

	wrmsr(0xC0000081, (0x8ULL << 32) | (0x20ULL << 48));

	wrmsr(0xC0000084, (1 << 9) | (1 << 10) | (1 << 8) | (1 << 18));

	wrmsr(0xC0000100, 0x201000);
	wrmsr(0xC0000101, 0x201000);

	asm volatile(
	        "mov %%cr0, %%rax\n\t"
	        "and $~(1 << 2), %%rax\n\t"  // clear EM
	        "or  $(1 << 1), %%rax\n\t"   // set MP
	        "mov %%rax, %%cr0\n\t"

	        "mov %%cr4, %%rax\n\t"
	        "or  $(1 << 9), %%rax\n\t"   // OSFXSR
	        "or  $(1 << 10), %%rax\n\t"  // OSXMMEXCPT
	        "mov %%rax, %%cr4\n\t"
	        :
	        :
	        : "rax", "memory"
	);
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len);
static uint64_t anon_allocate_syscall(size_t size, uint64_t *dk);
static uint64_t exit_syscall(uint64_t exit);
static uint64_t fork_syscall();
static uint64_t execve_syscall(const char *path, char **argv, char **evnp);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1,
                         uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	switch (index) {
		case 0:
			return read_syscall(arg0, (uint8_t *)arg1, arg2);
		case 1:
			return write_syscall(arg0, (uint8_t *)arg1, arg2);
		case 2:
			return anon_allocate_syscall(arg0, (uint64_t *)arg1);
		case 3:
			return exit_syscall(arg0);
		case 4:
			return fork_syscall();
		case 5:
			return execve_syscall((const char *)arg0, (char **)arg1, (char **)arg2);
		default:
			return (uint64_t) -1;
	}
}

static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len)
{
	return resource_read(fd, buf, len);
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len)
{
	return resource_write(fd, buf, len);
}

static uint64_t anon_allocate_syscall(size_t size, uint64_t *ptr)
{
	int pages = (size + 0xfff) >> 12;

	process *proc = get_current_process();

	uint64_t ret = proc->anon_allocate_end;

	for (int i = 0; i < pages; i++) {
		bitmap_allocator *ba = MUTEX_LOCK(g_ba);
		map_memory_page(ba, read_cr3(), proc->anon_allocate_end, 7);
		MUTEX_UNLOCK(g_ba);
		proc->anon_allocate_end += 0x1000;
	}

	*ptr = ret;
	return 0;
}

static uint64_t exit_syscall(uint64_t exit)
{
	process *process = get_current_process();
	// free all resources
	for (int i = 0; i < MAX_RESOURCES; i++) {
		resource_remove(i);
	}
	
	uint64_t cr3 = process->cr3;

	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	free_region(cr3, ba, 0x200000, 0x400000);
	free_region(cr3, ba, 0x400000, process->elf_end);
	free_region(cr3, ba, 0x800000, process->anon_allocate_end);
	MUTEX_UNLOCK(g_ba);


	// this doesnt return, it yields control to the scheduler
	// all resources need to be deleted here
	mark_current_proc_as_dead();
}

extern process *current_process;
static uint64_t fork_syscall()
{
	process *proc = get_current_process();
	uint64_t cr3 = proc->cr3;

	// allocate a new cr3
	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	uint64_t new_cr3 = (uintptr_t)allocate_page(ba);
	MUTEX_UNLOCK(g_ba);

	// mirror all the higher memory regions
	uint64_t *v_new_cr3 = P2V(new_cr3);
	uint64_t *v_cr3 = P2V(cr3);
	for (int i = 256; i < 512; i++) {
		v_new_cr3[i] = v_cr3[i];
	}

	ba = MUTEX_LOCK(g_ba);
	allocate_region(ba, new_cr3, 0x200000, proc->elf_end, 7);
	allocate_region(ba, new_cr3, 0x800000, proc->anon_allocate_end, 7);
	MUTEX_UNLOCK(g_ba);

	// now we have two identical address spaces
	// add a new process
	allocator *al = MUTEX_LOCK(g_al);
	process *p = alloc(al, sizeof(process));
	MUTEX_UNLOCK(g_al);

	memcpy(p, proc, sizeof(process));
	// adjust the cr3 of the new process
	p->cr3 = new_cr3;

	// now we pretend an intterupt happened right here
	uint64_t save_stack;
	cpu_status context;

	// fill the context
	asm volatile(
	        "movq %%rsp, (%0)\n\t"

	        "movq %1, %%rsp\n\t"

	        "pushq $0x10\n\t"
	        "pushq (%0)\n\t"
	        "pushfq\n\t"
	        "pushq $0x8\n\t"
	        "pushq %2\n\t"

	        "pushq %%rax\n\t"
	        "pushq %%rbx\n\t"
	        "pushq %%rcx\n\t"
	        "pushq %%rdx\n\t"
	        "pushq %%rbp\n\t"
	        "pushq %%rsi\n\t"
	        "pushq %%rdi\n\t"
	        "pushq %%r8\n\t"
	        "pushq %%r9\n\t"
	        "pushq %%r10\n\t"
	        "pushq %%r11\n\t"
	        "pushq %%r12\n\t"
	        "pushq %%r13\n\t"
	        "pushq %%r14\n\t"
	        "pushq %%r15\n\t"

	        "movq (%0), %%rsp\n\t"
	        :
	        : "r"(&save_stack),
	        "r"(((uint64_t)&context) + sizeof(cpu_status)),
	        "r"(&&__after_timer)
	        : "memory"
	);

__after_timer:
	// if we are the new process, we return
	if (get_current_process()->cr3 == new_cr3) {
		return 0;
	}

	// copy all the data between the page tables
	// including the iret frame, which is why the execution will
	// continue where we placed the int 0x20
	copy_pages_between_pagetables(new_cr3, cr3, 0x200000, proc->elf_end);
	copy_pages_between_pagetables(new_cr3, cr3, 0x800000, proc->anon_allocate_end);

	p->context = &context;

	insert_process(p);

	// now yield
	// the inserted process will always be behind the
	// current one in the linked list
	// meaning we reach __after_timer before
	// the stack becomes invalid
	asm volatile("int $0x20");

	return 1;
};

// this is where we search for the files
extern volatile struct limine_module_request module_request;
static uint64_t execve_syscall(const char *path, char **argv, char **envp) {
	// argv and envp are just ignored for now
	process *p = get_current_process();
	uint64_t cr3 = p->cr3;

	// copy the path onto the stack
	size_t path_len = strlen(path) + 1;
	if (path_len > 0x2000) 
		return (uint64_t)-1;
	char path_buf[path_len];
	memmove(path_buf, path, path_len);

	// first delete some things
	// namely the elf and allocated space
	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	free_region(cr3, ba, 0x400000, p->elf_end);
	free_region(cr3, ba, 0x800000, p->anon_allocate_end);
	MUTEX_UNLOCK(g_ba);

	// reset this counter
	p->anon_allocate_end = 0x800000;

	// map the elf too
	uint8_t *elf;
	int file_size = tar_lookup(module_request.response->modules[0]->address,
	                                path_buf, &elf);
	ba = MUTEX_LOCK(g_ba);
	p->elf_end = map_elf(ba, hhdm_request.response->offset, elf);
	MUTEX_UNLOCK(g_ba);

	elf_header *header = (elf_header *)(elf);

	asm volatile("cli");
	asm volatile(
	        "mov $0x202, %%r11\n\t"
	        "mov %0, %%rcx\n\t"
	        "mov $0x400000, %%rsp\n\t"
	        "sysretq\n\t"
	        :
	        : "r"(header->e_entry)
	        : "rcx", "r11", "memory"
	);

	for (;;);
	__builtin_unreachable();
}
