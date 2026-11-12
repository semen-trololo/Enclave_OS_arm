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

/* RAM: 512 МБ всего, low memory 0x0–0x7FFF зарезервировано */
/* Если .bss + стек не влезает — уменьши svc_stack или размеры остальных */
ASSERT(__bss_end < 0x20000000, "ERROR: kernel image exceeds RAM boundary")
