#include <elf.h>
#include <sched.h>
#include <memory.h>
#include <string.h>

void map_init_elf(bitmap_allocator *ba, uint64_t offset, uint8_t *elf) {
	elf_header *header = (elf_header *)(elf);
	elf_program *programs = (elf_program*)((uint8_t*)header + header->e_phoff);
	for (int i = 0; i < header->e_phnum; i++) {
		if (programs[i].p_type == 1) {
			int pages = ((programs[i].p_memsz + 0x1000 - 1) & ~(0x1000 - 1)) / 0x1000;
			for (int j = 0; j < pages; j++) {
				map_memory_page_current(ba, programs[i].p_vaddr + j * 0x1000, 7);
			}
			memcpy((void*)programs[i].p_vaddr,
			       (uint8_t*)header + programs[i].p_offset,
			       programs[i].p_filesz);
			
			if (programs[i].p_memsz > programs[i].p_filesz) {
			    memset((void*)(programs[i].p_vaddr + programs[i].p_filesz),
			           0,
			           programs[i].p_memsz - programs[i].p_filesz);
			}
		}
	}
}
