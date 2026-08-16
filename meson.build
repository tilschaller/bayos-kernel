project('kernel', 'c',
  default_options: ['warning_level=2', 'buildtype=release'],
)

nasm = find_program('nasm')

cc_args = [
  '-m64',
  '-nostdlib',
  '-fno-pie',
  '-fno-builtin',
  '-fno-exceptions',
  '-fno-unwind-tables',
  '-fno-asynchronous-unwind-tables',
  '-fno-ident',
  '-fno-stack-protector',
  '-mno-red-zone',
  '-mcmodel=kernel',
  '-mgeneral-regs-only',
  '-O2',
  '-nostdinc',
  '-ffreestanding',
  '-fno-stack-check',
  '-fno-lto',
  '-fno-PIC',
  '-ffunction-sections',
  '-fdata-sections',
]

inc = include_directories('include')

asm_srcs = files(
  'src/exception_handler.asm',
  'src/interrupt_handler.asm',
)

c_srcs = files(
  'src/main.c',
  'src/string.c',
  'src/framebuffer.c',
  'src/early.c',
  'src/interrupts.c',
  'src/gdt.c',
  'src/memory.c',
  'src/alloc.c',
  'src/sched.c',
  'src/keyboard.c',
  'src/io.c',
  'src/syscall.c',
  'src/fs/ustar.c',
  'src/elf.c',
)

nasm_gen = generator(nasm,
  output: '@BASENAME@.o',
  arguments: ['-f', 'elf64', '@INPUT@', '-o', '@OUTPUT@'],
)

asm_objs = nasm_gen.process(asm_srcs)

font_obj = files('assets/font.o')

kernel_ld = files('kernel.ld')

kernel = executable('kernel.elf',
  sources: c_srcs,
  objects: [asm_objs, font_obj],
  c_args: cc_args,
  include_directories: inc,
  link_args: ['-T', meson.current_source_dir() / 'kernel.ld', '-nostdlib', '-no-pie'],
  link_depends: kernel_ld,
  install: true,
  install_dir: 'boot',
)
