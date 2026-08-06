extern schedule
extern keyboard_handler
extern syscall_handler

global timer_int_handler
timer_int_handler:
	; acknowledge the interrupt
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax

	; store all registers on stack
	push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	; mov the stack pointer as argument to schedule function
	mov rdi, rsp
	; call the schedule function
	call schedule
	; returns a pointer to stack, restore it
	mov rsp, rax

	; restore all the registers
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax

	; return to the previous context
	iretq

global _keyboard_handler
_keyboard_handler:
	; acknowledge the interrupt
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax

	; store all registers on stack
	push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	in al, 0x60
	movzx rdi, al
	call keyboard_handler

	; restore all the registers
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax

	; return to the previous context
	iretq

global _syscall_handler
_syscall_handler:
	push rbp
	push r15
	push r14
	push r13
	push r12
	push r11
	push rcx

	push r9
	mov r9, r8
	mov r8, r10
	mov rcx, rdx
	mov rdx, rsi
	mov rsi, rdi
	mov rdi, rax

	call syscall_handler

	pop r9

	pop rcx
	pop r11
	pop r12
	pop r13
	pop r14
	pop r15
	pop rbp

	o64 sysret
