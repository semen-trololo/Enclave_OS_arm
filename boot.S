/* boot.S — Enclave OS, Raspberry Pi 1 B+ (BCM2835, ARM1176JZF-S) */

.section .text.boot
.global _start
_start:
    /* ==== ВХОД: PC=0x8000, SVC, IRQ/FIQ off, SP = мусор ==== */
    mov     r4, r2                  /* r4 := DTB; r4 не банкованный */

    mrs     r0, cpsr                /* trust but verify: глушим IRQ/FIQ */
    orr     r0, r0, #0xC0
    msr     cpsr_c, r0

    /* ---- стеки всем режимам ---- */
    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x12           /* IRQ */
    msr     cpsr_c, r0
    ldr     sp, =__irq_stack_top

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x11           /* FIQ */
    msr     cpsr_c, r0
    ldr     sp, =__fiq_stack_top

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x17           /* Abort */
    msr     cpsr_c, r0
    ldr     sp, =__abt_stack_top

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x1B           /* Undefined */
    msr     cpsr_c, r0
    ldr     sp, =__und_stack_top

    mrs     r0, cpsr
    bic     r0, r0, #0x1F
    orr     r0, r0, #0x13           /* SVC — стек ядра */
    msr     cpsr_c, r0
    ldr     sp, =__svc_stack_top

    /* ---- векторы на 0x00000000 ---- */
    ldr     r0, =_vectors_start
    ldr     r1, =_vectors_end
    mov     r2, #0
copy_vectors:
    ldr     r3, [r0], #4
    str     r3, [r2], #4
    cmp     r0, r1
    blo     copy_vectors

    /* ---- обнуление BSS ---- */
    ldr     r0, =__bss_start
    ldr     r1, =__bss_end
    mov     r2, #0
clear_bss:
    cmp     r0, r1
    bhs     bss_done
    str     r2, [r0], #4
    b       clear_bss
bss_done:

    /* ---- вызов kernel_main(0, 0xC42, dtb) по протоколу ARM Linux ---- */
    mov     r0, #0
    ldr     r1, =0xC42              /* не mov! 0xC42 не кодируется в ARM-imm */
    mov     r2, r4                  /* DTB из сохранённого r4 */
    bl      kernel_main

hang:                               /* ядро не должно возвращаться */
    b       hang

    .ltorg                          /* пул для всего кода выше */

/* ================= ТАБЛИЦА ВЕКТОРОВ ================= */
.align 2
.global _vectors_start
_vectors_start:
    ldr     pc, =_start             /* 0x00 Reset -> вход */
    ldr     pc, =undef_handler      /* 0x04 */
    ldr     pc, =svc_handler        /* 0x08 — будущий шлюз анклавов */
    ldr     pc, =pabt_handler       /* 0x0C */
    ldr     pc, =dabt_handler       /* 0x10 */
    nop                             /* 0x14 reserved */
    ldr     pc, =irq_handler        /* 0x18 */
    ldr     pc, =fiq_handler        /* 0x1C */
.ltorg
.global _vectors_end
_vectors_end:                       /* ВАЖНО: ПОСЛЕ .ltorg! */

/* ================= СТЕКИ (в .bss — не раздувают файл) ================= */
.section .bss
.align 3                            /* 2^3 = 8 байт (AAPCS) */
irq_stack:  .skip 4096              /* по странице на режим */
__irq_stack_top:
fiq_stack:  .skip 4096
__fiq_stack_top:
abt_stack:  .skip 4096
__abt_stack_top:
und_stack:  .skip 4096
__und_stack_top:
svc_stack:  .skip 65536             /* 64 КБ ядру на boot-этап */
__svc_stack_top:
