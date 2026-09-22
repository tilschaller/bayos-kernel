#include <limine.h>
#include <framebuffer.h>
#include <early.h>
#include <interrupts.h>
#include <gdt.h>
#include <memory.h>
#include <stdint.h>
#include <alloc.h>
#include <sched.h>
#include <io.h>
#include <keyboard.h>
#include <syscall.h>
#include <fs/ustar.h>
#include <elf.h>
#include <string.h>

//
// here we define what the limine bootloader should provide for us
//
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(4);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
	.id = LIMINE_RSDP_REQUEST_ID,
	.revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST_ID,
	.revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memory_map_request = {
	.id = LIMINE_MEMMAP_REQUEST_ID,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
	.id = LIMINE_HHDM_REQUEST_ID,
	.revision = 0,
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = {
	.id = LIMINE_MODULE_REQUEST_ID,
	.revision = 0,
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
        LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
        LIMINE_REQUESTS_END_MARKER;

//
// the global mutexes of objects created in main.c
//
Mutex(framebuffer) g_fb;
Mutex(bitmap_allocator) g_ba;
Mutex(allocator) g_al;


void enter_ring_3_init(void);
uint8_t *init_elf;

//
// this is the entry point called by the bootloader
// this function should perform all necessary initialization
// set up the scheduler, and then pass control to it
//
__attribute__((noreturn))
void _start(void)
{
	//
	// the least we need from the bootlaoder
	// 1. is the revision 4 of the limine protocol
	// (since we need features from that)
	// 2.  a framebuffer
	// if one of these is not met, we halt execution
	if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) ||
	    framebuffer_request.response == NULL ||
	    framebuffer_request.response->framebuffer_count < 1
	   )
		asm volatile("hlt");

	//
	// first we init the framebuffer to be able to print out error messages
	// for this we use the first framebuffer provided by limine
	//
	framebuffer fb;
	framebuffer_init(framebuffer_request.response->framebuffers[0], &fb);

	//
	// next we init a early_printk, which should only be used
	// in this function and ignored afterwards
	//
	early_printk_init(&fb);
	early_printk("OS for BS\n");

	//
	// the next step is setting up valid exceptions
	// so the cpu can never crash without outputting some info
	//
	exceptions_init();
	early_printk("[OK] Exceptions\n");

	//
	// next we need to set up our own gdts
	//
	gdt_init();
	early_printk("[OK] GDT\n");

	//
	// then we set up the physical memory managment
	//
	// for this we need hhdm and the memory map
	if (memory_map_request.response == NULL) {
		early_printk("[FAIL] no memory map provided by limine\n");
		asm volatile("hlt");
	}
	if (hhdm_request.response == NULL) {
		early_printk("[FAIL] no hhdm mapping provided by limine\n");
		asm volatile("hlt");
	}

	bitmap_allocator page_allocator;
	bitmap_allocator_init(memory_map_request.response,
	                      hhdm_request.response->offset, &page_allocator);
	early_printk("[OK] Bitmap Allocator\n");

	//
	// then we need to allocate space for the heap
	//
	allocate_region(&page_allocator, read_cr3(), 0xffffffff80000000 - 0x200000,
	                0xffffffff80000000, 3);
	early_printk("[OK] Heap\n");

	//
	// with this space we can actually create the allocator
	//
	allocator al;
	new_allocator(0xffffffff80000000 - 0x200000, 0x200000, &al);
	early_printk("[OK] Allocator\n");

	//
	// next we need to set up the scheduler
	//
	sched_init(&al);
	early_printk("[OK] Scheduler\n");

	//
	// here we can check for modules
	// while we still have access to
	// early_printk();
	//
	if (!module_request.response || module_request.response->module_count == 0) {
		early_printk("[FAIL] no initramfs provided\n");
		asm volatile("hlt");
	}

	//
	// find the init process
	//
	int init_file_size = tar_lookup(module_request.response->modules[0]->address,
	                                "/usr/bin/init", &init_elf);
	if (!init_file_size) {
		early_printk("[FAIL] no init process in initramfs\n");
		asm volatile("hlt");
	}

	//
	// last we expose the data structures we created here as mutexes
	// to global variables, so we can use them
	// the stack is preserved, so it can just stay here
	// this includes:
	// - framebuffer fb,
	// - bitmap_allocator page_allocator,
	// - allocator al
	//
	g_fb.data = &fb;
	g_fb.lock = mutex_create();
	g_ba.data = &page_allocator;
	g_ba.lock = mutex_create();
	g_al.data = &al;
	g_al.lock = mutex_create();
	early_printk("[OK] Mutexes\n");

	//
	// then we start up the scheduler
	// this remains as a process
	// but more of a fallback process
	// if everything other fails
	// NOTE: dont use early_printk anymore, it doesnt respect the framebuffer mutex
	//
	early_printk("Giving up execution to scheduler\n");
	asm volatile("sti");

	keyboard_process_init();
	syscall_init();

	g_framebuffer_print_pipe = pipe_create(0x100);
	add_process((uintptr_t)&framebuffer_print_process);

	add_process((uintptr_t)&enter_ring_3_init);

	//
	// this will continue running as a fallback
	// if all other process are blocked for some reason
	//
	for (;;);
	__builtin_unreachable();
}

__attribute__((noreturn))
void enter_ring_3_init(void)
{
	resource_add(PIPE, g_keyboard_pipe);
	resource_add(PIPE, g_framebuffer_print_pipe);

	//
	// map the stack of the init process into our address space
	// this creates 2 MiB stack at 2MiB
	//
	bitmap_allocator *ba = MUTEX_LOCK(g_ba);
	allocate_region(ba, read_cr3(), 0x200000, 0x400000, 7);
	MUTEX_UNLOCK(g_ba);

	//
	// then we need to copy the elf file
	//
	ba = MUTEX_LOCK(g_ba);
	get_current_process()->elf_end = map_elf(ba, hhdm_request.response->offset,
	        init_elf);
	MUTEX_UNLOCK(g_ba);

	elf_header *header = (elf_header *)(init_elf);

	process *proc = get_current_process();
	proc->pid = 1;

	// something needed for rtld i think
	memset((void*)0x200000, 0, 0x200000);
	*(uint64_t *)0x201000 = 0x201000;

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
