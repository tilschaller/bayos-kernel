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
void bitmap_allocator_init(struct limine_memmap_response *memmap,
                           uint64_t offset, bitmap_allocator *ba);

//
// return and free physical pages
//
void *allocate_page(bitmap_allocator *ba);
void free_page(bitmap_allocator *ba, void *page);

//
// map_memory_function
//
// maps one page of memory into the current address space
//
void map_memory_page_current(bitmap_allocator *ba, uintptr_t virt, int flags);

DEFINE_MUTEX_TYPE(bitmap_allocator);
extern Mutex(bitmap_allocator) g_ba;

typedef uint64_t pte_t;
typedef pte_t *pagetable_t;


extern volatile struct limine_hhdm_request hhdm_request;
#define P2V(pa) ((void *)(pa + hhdm_request.response->offset))

void allocate_region_current(bitmap_allocator *ba, uintptr_t start,
                             uintptr_t end, int flags);
void free_region(uint64_t pml4_phys, bitmap_allocator *ba, uint64_t start,
                 uint64_t end);

#endif // _MEMORY_H
