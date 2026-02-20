; boot.asm - GRUB Multiboot entry point
; Assembler: NASM
; Target: x86 32-bit Protected Mode

MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_USE_GFX       equ 0
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

[BITS 32]
[GLOBAL mboot]
[GLOBAL start]
[EXTERN kernel_main]

section .multiboot
align 4
mboot:
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KB stack
stack_top:

section .text
start:
    cli                  ; Disable interrupts
    mov esp, stack_top   ; Setup stack

    ; Push multiboot info pointers for kernel_main
    push ebx             ; Multiboot info pointer
    push eax             ; Multiboot magic

    call kernel_main     ; Call C kernel

    ; If kernel_main returns, halt
.hang:
    hlt
    jmp .hang
