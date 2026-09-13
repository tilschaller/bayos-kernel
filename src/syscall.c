#include <syscall.h>
#include <stdint.h>
#include <framebuffer.h>
#include <sched.h>
#include <keyboard.h>
#include <memory.h>
#include <io.h>

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
static uint64_t fork_syscall(int pid);

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
			return fork_syscall((int)arg0);
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
		map_memory_page_current(ba, proc->anon_allocate_end, 7);
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

	unsigned long save = save_irqdisable();
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	irqrestore(save);

	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	free_region(cr3, ba, 0x200000, 0x400000);
	free_region(cr3, ba, 0x400000, process->elf_end);
	free_region(cr3, ba, 0x800000, process->anon_allocate_end);
	MUTEX_UNLOCK(g_ba);


	// this doesnt return, it yields control to the scheduler
	// all resources need to be deleted here
	mark_current_proc_as_dead();
}

static uint64_t fork_syscall(int pid)
{
	// first get the current cr3
	unsigned long save = save_irqdisable();
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	irqrestore(save);

	// allocate a new cr3
	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	uint64_t new_cr3 = (uintptr_t)allocate_page(ba);
	MUTEX_UNLOCK(g_ba);

	// map the kernel mappings
	uint64_t *v_new_cr3 = P2V(new_cr3);
	uint64_t *v_cr3 = P2V(cr3);
	for (int i = 256; i < 512; i++) {
		v_new_cr3[i] = v_cr3[i];
	}
}
