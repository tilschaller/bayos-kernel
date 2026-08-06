#include <syscall.h>
#include <stdint.h>
#include <framebuffer.h>
#include <sched.h>
#include <keyboard.h>

extern void _syscall_handler;

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo;
    uint32_t hi;

    asm volatile (
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

    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(lo), "d"(hi)
        : "memory"
    );
}

void syscall_init(void) {
	// enable syscalls
	uint64_t efer = rdmsr(0xC0000080);
	wrmsr(0xC0000080, efer | 1);

	wrmsr(0xC0000082, (uintptr_t)&_syscall_handler);

	wrmsr(0xC0000081, (0x8ULL << 32) | (0x20ULL << 48));

	wrmsr(0xC0000084, (1 << 9) | (1 << 10) | (1 << 8) | (1 << 18));

	wrmsr(0xC0000100, 0x1fc000);
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len);
static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len);

uint64_t syscall_handler(uint64_t index, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (index) {
    case 0:
        return read_syscall(arg0, (uint8_t*)arg1, arg2);
    case 1:
        return write_syscall(arg0, (uint8_t*)arg1, arg2);
    default:
        return (uint64_t)-1;
    }
}

static uint64_t read_syscall(uint64_t fd, uint8_t *buf, size_t len) {
    return resource_read(fd, buf, len);
}

static uint64_t write_syscall(uint64_t fd, const uint8_t *buf, size_t len) {
    return resource_write(fd, buf, len);
}
