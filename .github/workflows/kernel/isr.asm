; isr.asm - Interrupt Service Routine stubs

[EXTERN isr_handler]
[EXTERN irq_handler]

; Common ISR stub - saves registers, calls C handler
isr_common_stub:
    pusha               ; Push edi,esi,ebp,esp,ebx,edx,ecx,eax
    mov ax, ds
    push eax            ; Save data segment

    mov ax, 0x10        ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax             ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8          ; Clean error code and ISR number
    iret

; Common IRQ stub
irq_common_stub:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq_handler

    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    popa
    add esp, 8
    iret

; Macro for ISR without error code
%macro ISR_NOERR 1
[GLOBAL isr%1]
isr%1:
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

; Macro for ISR with error code
%macro ISR_ERR 1
[GLOBAL isr%1]
isr%1:
    push byte %1
    jmp isr_common_stub
%endmacro

; Macro for IRQ
%macro IRQ 2
[GLOBAL irq%1]
irq%1:
    push byte 0
    push byte %2
    jmp irq_common_stub
%endmacro

; CPU Exceptions
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQs (remapped to 32-47)
IRQ  0, 32   ; Timer
IRQ  1, 33   ; Keyboard
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44  ; PS/2 Mouse
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
