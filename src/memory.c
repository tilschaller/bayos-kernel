#include <memory.h>
#include <string.h>
#include <io.h>
#include <limine.h>

// 
// helper functions for bitmap_allocator_init()
//
// how many bytes do we need for this particular bitmap
static inline size_t bitmap_bytes_for(size_t page_count) {
	return (page_count + 7) / 8;
}
// given a number of pages, compute, how many pages are needed to store information about this segment
static size_t calc_header_pages(size_t page_count) {
	size_t header_pages = 1;
	size_t prev;

	do {
		prev = header_pages;
		size_t usable_pages = page_count - header_pages;
		size_t needed_bytes = OFFSET_OF_DATA_IN_AREA + bitmap_bytes_for(usable_pages);
		header_pages = (needed_bytes + 0x1000 - 1) / 0x1000;
		if (header_pages == 0) header_pages = 1;
	} while (header_pages != prev);

	return header_pages;
}

//
// this creates a new bitmap allocator 
// using the hhdm memory offset
// it returns the pages as physical addresses though
//
void bitmap_allocator_init(struct limine_memmap_response *memmap, uint64_t offset, bitmap_allocator *ba) {
	size_t entry_count = 0;
	for (int i = 0; i < (int)memmap->entry_count; i++) {
		struct limine_memmap_entry *entry = memmap->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= 0x2000) {
			size_t total_pages = entry->length / 0x1000;
			size_t header_pages = calc_header_pages(total_pages);
			size_t usable_pages = total_pages - header_pages;

			ba->areas[entry_count] = (void*)(entry->base + offset);

			memset(ba->areas[entry_count], 0, header_pages * 0x1000);

			ba->areas[entry_count]->first_page = (void*)(entry->base + header_pages * 0x1000);
			ba->areas[entry_count]->pages_count = usable_pages;
			ba->areas[entry_count]->used = 0;

			entry_count++;
		}
	}
	ba->areas_count = entry_count;
}

//
// helper function to allocate physical pages
//
// return the index of the unused page in the bitmap
// the bitmap must contain an unused page
// and mark as used
static int find_unused_page(uint8_t *bitmap, size_t len) {
	for (size_t i = 0; i < len; i++) {
		for (int j = 0; j < 8; j++) {
			int val = (bitmap[i] >> j) & 1;
			if (val == 0) {
				bitmap[i] |= 1 << j;
				return i * 8 + j;
			}
		}
	}

	return -1;
}

//
// these functions allocator and free physical pages
// using the bitmap allocator
//
void *allocate_page(bitmap_allocator *ba) {
	for (size_t i = 0; i < ba->areas_count; i++) {
		if (ba->areas[i]->pages_count != ba->areas[i]->used) {
			int index = find_unused_page(ba->areas[i]->data, bitmap_bytes_for(ba->areas[i]->pages_count));
			ba->areas[i]->used++;
			return (void*)((uintptr_t)ba->areas[i]->first_page + index * 0x1000);
		}
	}

	return NULL;
}

// we just assume the address is proper
void free_page(bitmap_allocator *ba, void *page) {
	// first check, if the area contains the page we want to free
	for (size_t i = 0; i < ba->areas_count; i++) {
		if (page >= ba->areas[i]->first_page && page < ba->areas[i]->first_page + ba->areas[i]->pages_count * 0x1000) {
			// the page is in this area of memory
			ba->areas[i]->used--;
			int index = (page - ba->areas[i]->first_page) / 0x1000;
			int array_index = index / 8;
			int array_index_offset = index % 8;
			ba->areas[i]->data[array_index] &= ~(1 << array_index_offset);
			return;
		}
	}
}

#define HEAP_SIZE (2 * 1024 * 1024) // 2MiB
#define HEAP_ADDR (0xffffffff80000000 - HEAP_SIZE)
#define HEAP_FLAGS 3
#define PAGE_ADDR_MASK 0x000ffffffffff000ULL

static inline void invlpg(void *p) {
	asm volatile("invlpg (%0)" : : "b"(p) : "memory");
}

void allocate_heap(uint64_t offset, bitmap_allocator *ba) {
	// first we need to get cr3
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	// first we need to check if the needed pages already exist
	size_t pml4_index = (HEAP_ADDR >> 39) & 0x1ff;
	size_t pdp_index = (HEAP_ADDR >> 30) & 0x1ff;
	size_t pd_index = (HEAP_ADDR >> 21) & 0x1ff;

	uint64_t *pml4 = (uint64_t*)(cr3 + offset);
	if (pml4[pml4_index] == 0) {
		pml4[pml4_index] = (uintptr_t)allocate_page(ba) | HEAP_FLAGS;
		memset((void*)((pml4[pml4_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pdp = (uint64_t*)((pml4[pml4_index] & PAGE_ADDR_MASK) + offset);
	if (pdp[pdp_index] == 0) {
		pdp[pdp_index] = (uintptr_t)allocate_page(ba) | HEAP_FLAGS;
		memset((void*)((pdp[pdp_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pd = (uint64_t*)((pdp[pdp_index] & PAGE_ADDR_MASK) + offset);
	if (pd[pd_index] == 0) {
		pd[pd_index] = (uintptr_t)allocate_page(ba) | HEAP_FLAGS;
		memset((void*)((pd[pd_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pt = (uint64_t*)((pd[pd_index] & PAGE_ADDR_MASK) + offset);
	for (int i = 0; i < HEAP_SIZE / 0x1000; i++) {
		pt[i] = (uintptr_t)allocate_page(ba) | HEAP_FLAGS;
		invlpg((void*)(HEAP_ADDR + 0x1000 * i));
	}

	memset((void*)HEAP_ADDR, 0, HEAP_SIZE);
}

#define INIT_STACK_SIZE (2 * 1024 * 1024) // 2MiB
#define INIT_STACK_ADDR (2 * 1024 * 1024) // at 2MiB
#define INIT_STACK_FLAGS 7

void map_init_stack(uint64_t offset, bitmap_allocator *ba) {
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	size_t pml4_index = (INIT_STACK_ADDR >> 39) & 0x1ff;
	size_t pdp_index = (INIT_STACK_ADDR >> 30) & 0x1ff;
	size_t pd_index = (INIT_STACK_ADDR >> 21) & 0x1ff;

	uint64_t *pml4 = (uint64_t*)(cr3 + offset);
	if (pml4[pml4_index] == 0) {
		pml4[pml4_index] = (uintptr_t)allocate_page(ba) | INIT_STACK_FLAGS;
		memset((void*)((pml4[pml4_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pdp = (uint64_t*)((pml4[pml4_index] & PAGE_ADDR_MASK) + offset);
	if (pdp[pdp_index] == 0) {
		pdp[pdp_index] = (uintptr_t)allocate_page(ba) | INIT_STACK_FLAGS;
		memset((void*)((pdp[pdp_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pd = (uint64_t*)((pdp[pdp_index] & PAGE_ADDR_MASK) + offset);
	if (pd[pd_index] == 0) {
		pd[pd_index] = (uintptr_t)allocate_page(ba) | INIT_STACK_FLAGS;
		memset((void*)((pd[pd_index] & PAGE_ADDR_MASK) + offset), 0, 0x1000);
	}

	uint64_t *pt = (uint64_t*)((pd[pd_index] & PAGE_ADDR_MASK) + offset);
	for (int i = 0; i < INIT_STACK_SIZE / 0x1000; i++) {
		pt[i] = (uintptr_t)allocate_page(ba) | INIT_STACK_FLAGS;
		invlpg((void*)(INIT_STACK_ADDR + 0x1000 * i));
	}

	memset((void*)INIT_STACK_ADDR, 0, INIT_STACK_SIZE);
}

extern volatile struct limine_hhdm_request hhdm_request;

static inline uintptr_t alloc_page_or_halt(bitmap_allocator *ba) {
	uintptr_t frame = (uintptr_t)allocate_page(ba);
	if (frame == 0) {
		asm volatile("cli; hlt");
		__builtin_unreachable();
	}
	return frame;
}

void map_memory_page_current(bitmap_allocator *ba, uintptr_t virt, int flags) {
	unsigned long save = save_irqdisable();

	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	irqrestore(save);

	size_t pml4_index = (virt >> 39) & 0x1ff;
	size_t pdp_index  = (virt >> 30) & 0x1ff;
	size_t pd_index   = (virt >> 21) & 0x1ff;
	size_t pt_index   = (virt >> 12) & 0x1ff;

	uint8_t *offset = (uint8_t*)hhdm_request.response->offset;

	uint64_t *pml4 = (uint64_t*)(cr3 + offset);
	if ((pml4[pml4_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pml4[pml4_index] = frame | flags;
	}

	uint64_t *pdp = (uint64_t*)(offset + (pml4[pml4_index] & ~0xfffULL));
	if ((pdp[pdp_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pdp[pdp_index] = frame | flags;
	}

	uint64_t *pd = (uint64_t*)(offset + (pdp[pdp_index] & ~0xfffULL));
	if ((pd[pd_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pd[pd_index] = frame | flags;
	}

	uint64_t *pt = (uint64_t*)(offset + (pd[pd_index] & ~0xfffULL));
	if ((pt[pt_index] & 1) == 0) 
		pt[pt_index] = alloc_page_or_halt(ba) | flags;

	invlpg((void*)virt);
}