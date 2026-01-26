.section .text

.extern __fuzz_harness_main
.extern __libc_start_main
.extern main

.global __fuzz_harness_start
__fuzz_harness_start:
    xor %ebp, %ebp
    mov %rdx, %r9
    pop %rsi
    mov %rsp, %rdx
    and $-16, %rsp
    push %rax
    push %rsp
    xor %r8d, %r8d
    xor %ecx, %ecx
    mov __fuzz_harness_main@GOTPCREL(%rip), %rdi
    call __libc_start_main@PLT
    hlt 

.global __fuzz_harness_main_fn
__fuzz_harness_main_fn:
    jmp main@PLT
