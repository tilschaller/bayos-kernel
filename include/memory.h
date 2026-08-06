#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <limine.h>
#include <sched.h>

// TODO: performance improvements 
// (maybe as suggested here: https://wiki.osdev.org/Page_Frame_Allocation#Bitmap)
// please remember to change this, when modifying bitmap_allocator
#define OFFSET_OF_DATA_IN_AREA 24
typedef struct {
	size_t areas_count;
	struct {
		void *first_page;
		size_t pages_count;
		size_t used;
		uint8_t data[];
	} *areas[0x100];
	// this array should be large enough for most memory layouts
} bitmap_allocator;

//
// this creates a new bitmap allocator 
// using the hhdm memory offset
// it returns the pages as physical addresses though
//
void bitmap_allocator_init(struct limine_memmap_response *memmap, uint64_t offset, bitmap_allocator *ba);

//
// return and free physical pages
//
void *allocate_page(bitmap_allocator *ba);
void free_page(bitmap_allocator *ba, void *page);

//
// allocate 2mb of pages for the heap using a bitmap allocator
// NOTE: only call this once, at boot time
//
// please dont just change these, without understanding what the function does
// and checking if this will break anything
#define HEAP_SIZE (2 * 1024 * 1024) // 2MiB
#define HEAP_ADDR (0xffffffff80000000 - HEAP_SIZE)
void allocate_heap(uint64_t offset, bitmap_allocator *ba);
void map_init_stack(uint64_t offset, bitmap_allocator *ba);

//
// map_memory_function
//
// maps one page of memory into the current address space
//
void map_memory_page_current(bitmap_allocator *ba, uintptr_t virt, int flags);

DEFINE_MUTEX_TYPE(bitmap_allocator);
extern Mutex(bitmap_allocator) g_ba;

#endif // _MEMORY_H
