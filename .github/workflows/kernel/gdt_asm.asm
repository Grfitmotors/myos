; gdt.asm - Global Descriptor Table setup

[GLOBAL gdt_flush]

gdt_flush:
    mov eax, [esp+4]    ; Get GDT pointer from argument
    lgdt [eax]          ; Load GDT

    mov ax, 0x10        ; 0x10 = kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush     ; Far jump to reload CS with kernel code segment
.flush:
    ret

[GLOBAL idt_flush]

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret
