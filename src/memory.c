#include <memory.h>
#include <string.h>
#include <io.h>
#include <limine.h>

//
// helper functions for bitmap_allocator_init()
//
// how many bytes do we need for this particular bitmap
static inline size_t bitmap_bytes_for(size_t page_count)
{
	return (page_count + 7) / 8;
}
// given a number of pages, compute, how many pages are needed to store information about this segment
static size_t calc_header_pages(size_t page_count)
{
	size_t header_pages = 1;
	size_t prev;

	do {
		prev = header_pages;
		size_t usable_pages = page_count - header_pages;
		size_t needed_bytes = OFFSET_OF_DATA_IN_AREA + bitmap_bytes_for(usable_pages);
		header_pages = (needed_bytes + 0x1000 - 1) / 0x1000;
		if (header_pages == 0) header_pages = 1;
	}
	while (header_pages != prev);

	return header_pages;
}

//
// this creates a new bitmap allocator
// using the hhdm memory offset
// it returns the pages as physical addresses though
//
void bitmap_allocator_init(struct limine_memmap_response *memmap,
                           uint64_t offset, bitmap_allocator *ba)
{
	size_t entry_count = 0;
	for (int i = 0; i < (int)memmap->entry_count; i++) {
		struct limine_memmap_entry *entry = memmap->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= 0x2000) {
			size_t total_pages = entry->length / 0x1000;
			size_t header_pages = calc_header_pages(total_pages);
			size_t usable_pages = total_pages - header_pages;

			ba->areas[entry_count] = (void *)(entry->base + offset);

			memset(ba->areas[entry_count], 0, header_pages * 0x1000);

			ba->areas[entry_count]->first_page = (void *)(entry->base + header_pages *
			        0x1000);
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
static int find_unused_page(uint8_t *bitmap, size_t len)
{
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
void *allocate_page(bitmap_allocator *ba)
{
	for (size_t i = 0; i < ba->areas_count; i++) {
		if (ba->areas[i]->pages_count != ba->areas[i]->used) {
			int index = find_unused_page(ba->areas[i]->data,
			                             bitmap_bytes_for(ba->areas[i]->pages_count));
			ba->areas[i]->used++;
			return (void *)((uintptr_t)ba->areas[i]->first_page + index * 0x1000);
		}
	}

	return NULL;
}

// we just assume the address is proper
void free_page(bitmap_allocator *ba, void *page)
{
	// first check, if the area contains the page we want to free
	for (size_t i = 0; i < ba->areas_count; i++) {
		if (page >= ba->areas[i]->first_page
		    && page < ba->areas[i]->first_page + ba->areas[i]->pages_count * 0x1000) {
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

static inline void invlpg(void *p)
{
	asm volatile("invlpg (%0)" : : "b"(p) : "memory");
}

static inline uintptr_t alloc_page_or_halt(bitmap_allocator *ba)
{
	uintptr_t frame = (uintptr_t)allocate_page(ba);
	if (frame == 0) {
		asm volatile("cli; hlt");
		__builtin_unreachable();
	}
	return frame;
}

void map_memory_page_current(bitmap_allocator *ba, uintptr_t virt, int flags)
{
	unsigned long save = save_irqdisable();

	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));

	irqrestore(save);

	size_t pml4_index = (virt >> 39) & 0x1ff;
	size_t pdp_index  = (virt >> 30) & 0x1ff;
	size_t pd_index   = (virt >> 21) & 0x1ff;
	size_t pt_index   = (virt >> 12) & 0x1ff;

	uint8_t *offset = (uint8_t *)hhdm_request.response->offset;

	uint64_t *pml4 = (uint64_t *)(cr3 + offset);
	if ((pml4[pml4_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pml4[pml4_index] = frame | flags;
	}

	uint64_t *pdp = (uint64_t *)(offset + (pml4[pml4_index] & ~0xfffULL));
	if ((pdp[pdp_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pdp[pdp_index] = frame | flags;
	}

	uint64_t *pd = (uint64_t *)(offset + (pdp[pdp_index] & ~0xfffULL));
	if ((pd[pd_index] & 1) == 0) {
		uintptr_t frame = alloc_page_or_halt(ba);
		memset(offset + frame, 0, 0x1000);
		pd[pd_index] = frame | flags;
	}

	uint64_t *pt = (uint64_t *)(offset + (pd[pd_index] & ~0xfffULL));
	if ((pt[pt_index] & 1) == 0)
		pt[pt_index] = alloc_page_or_halt(ba) | flags;

	invlpg((void *)virt);
}



#define PAGE_SIZE      4096UL
#define PAGE_SHIFT     12
#define PT_INDEX_BITS  9
#define PT_INDEX_MASK  0x1FFUL

#define PTE_P   (1UL << 0)   // Present
#define PTE_W   (1UL << 1)   // Writable
#define PTE_U   (1UL << 2)   // User accessible
#define PTE_PS  (1UL << 7)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000UL
#define PTE_ADDR(pte) ((pte) & PTE_ADDR_MASK)

typedef uint64_t pte_t;
typedef pte_t *pagetable_t;

static inline uint64_t pt_index(uint64_t va, int level)
{
	return (va >> (PAGE_SHIFT + PT_INDEX_BITS * level)) & PT_INDEX_MASK;
}

static pte_t *walk(uint64_t pml4_phys, uint64_t va)
{
	pte_t *table = (pte_t *)P2V(pml4_phys);

	// Levels 3, 2, 1 are the PML4, PDPT, and PD -- each just points to
	// the next table down.
	for (int level = 3; level >= 1; level--) {
		pte_t *entry = &table[pt_index(va, level)];

		if (!(*entry & PTE_P)) {
			return NULL; // nothing mapped here at all
		}

		if (*entry & PTE_PS) {
			// Huge page at this level -- not a 4K leaf PTE.
			// Bail out; caller isn't set up to handle this.
			return NULL;
		}

		table = (pte_t *)P2V(PTE_ADDR(*entry));
	}

	// level 0: the actual page table, holding 4K page leaf entries
	return &table[pt_index(va, 0)];
}

#define PAGE_ROUND_DOWN(a) ((a) & ~(PAGE_SIZE - 1))

void free_region(uint64_t pml4_phys, bitmap_allocator *ba,
                 uint64_t start, uint64_t end)
{
	start = PAGE_ROUND_DOWN(start);

	for (uint64_t va = start; va < end; va += PAGE_SIZE) {
		pte_t *pte = walk(pml4_phys, va);

		if (pte == NULL || !(*pte & PTE_P)) {
			continue; // not mapped, nothing to free
		}

		uint64_t phys_addr = PTE_ADDR(*pte);

		free_page(ba, (void *)phys_addr);

		*pte = 0; // unmap
	}
}

void allocate_region_current(bitmap_allocator *ba, uintptr_t start,
                             uintptr_t end, int flags)
{
	start = PAGE_ROUND_DOWN(start);

	for (uintptr_t va = start; va < end; va += PAGE_SIZE) {
		map_memory_page_current(ba, va, flags);
	}
}
