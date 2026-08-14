ENTRY(_start)

SECTIONS
{
    . = 0x8000;                     /* адрес загрузки — туда, куда кладёт GPU */

    .text : {
        KEEP(*(.text.boot))         /* boot-код первым и не выбрасывать */
        *(.text*)
    }

    . = ALIGN(8);
    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    . = ALIGN(8);
    __bss_start = .;                /* символы для clear_bss в boot.S */
    .bss : { *(.bss*) *(COMMON) }
    __bss_end = .;

    __kernel_end = .;
}
