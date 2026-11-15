/* linker.ld — Enclave OS, Raspberry Pi 1 B+
   GPU кладёт kernel.img по 0x8000; low memory 0x0..0x7FFF — векторы/ATAG. */

ENTRY(_start)

MEMORY { RAM (rwx) : ORIGIN = 0x00008000, LENGTH = 512M - 0x8000 }

SECTIONS
{
    . = 0x8000;

    .text : {
        KEEP(*(.text.boot))         /* boot-код первым и не выбрасывать */
        *(.text*)
    } > RAM

    . = ALIGN(8);
    .rodata : { *(.rodata*) } > RAM

    . = ALIGN(8);
    .data : { *(.data*) } > RAM

    . = ALIGN(8);
    .bss (NOLOAD) : {
        __bss_start = .;            /* символы для clear_bss в boot.S */
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    __kernel_end = .;

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.comment)
    }
}

/* Рано ловим ошибки раскладки, а не на плате */
ASSERT(_start == 0x8000, "ERROR: _start must be at 0x8000")
ASSERT(__kernel_end - 0x8000 < 8M, "ERROR: kernel image > 8 MB")
ASSERT(__bss_end < 0x20000000, "ERROR: kernel exceeds RAM boundary")
