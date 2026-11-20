/* Multiboot2 header — must appear in the first 32 KiB of the kernel image. */
.section .multiboot
.align 8
multiboot_header:
    .long 0xE85250D6              /* magic */
    .long 0                       /* architecture: i386 */
    .long multiboot_header_end - multiboot_header
    .long -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))
    /* end tag */
    .word 0
    .word 0
    .long 8
multiboot_header_end:

.section .text
.global _start
_start:
    cli
    mov $stack_top, %esp
    call kmain
1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
