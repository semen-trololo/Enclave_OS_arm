/* boot.S — Enclave OS, Raspberry Pi 1 B+ (BCM2835, ARM1176JZF-S) */

.section .text.boot
.global _start
_start:
    /* ==== ВХОД: PC=0x8000, SVC, IRQ/FIQ off, SP = мусор ==== */
    mov     r4, r2                  /* r4 := ATAGS/DTB; r4 не банкованный */
    mov     r5, r1                  /* r5 := machine type от firmware */

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

    /* ---- вызов kernel_main(r0, r1, r2) как их принёс firmware ----
       firmware уже передал r0=0, r1=machine_type, r2=ATAGS/DTB.
       Сохранённые r5/r4 — оригинальные значения, r0 не перетираем. */
    mov     r1, r5                  /* machine type: реальный регистр, не константа */
    mov     r2, r4                  /* ATAGS/DTB из сохранённого r4 */
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

/* ============ ОБРАБОТЧИКИ ИСКЛЮЧЕНИЙ (M1) ============
   Стоят после _vectors_end — иначе копирка векторов утащила бы их на 0x0.

   Кадр на стеке режима (64 байта, порядок обязан совпадать с
   struct exc_frame в kernel_main.c):
       [sp + 0  .. 51]  r0..r12
       [sp + 52]        SPSR режима
       [sp + 56]        LR_<режима> как его принёс процессор
       [sp + 60]        PC проблемной инструкции = LR - delta

   delta: у undef/svc/pabt/irq/fiq возвратный LR завышен на 4, у data
   abort — на 8. Возврата из обработчика нет намеренно: сначала должен
   появиться планировщик и контекст анклава (M6).

   Кроме паники обработчик умеет ловить пробные исключения самопроверки:
   если ядро выставило exc_probe, обработчик пишет номер в exc_caught и
   возвращается на инструкцию СРАЗУ ПОСЛЕ ловушки (movs pc восстанавливает
   CPSR из SPSR). Без exc_probe любое исключение = panic. */

.macro EXC_HANDLER label, tag, tagid, delta
.align 2
.global \label
\label:
    sub     sp, sp, #64             /* стек режима уже задан в _start */
    stmia   sp, {r0-r12}            /* маскируем прерывания уже после сохранения: r0 ещё настоящий */
    mrs     r0, cpsr
    orr     r0, r0, #0xC0
    msr     cpsr_c, r0
    add     r12, sp, #52
    mrs     r0, spsr
    str     r0, [r12], #4           /* SPSR */
    str     lr, [r12], #4           /* LR_<mode> */
    sub     r0, lr, #\delta
    str     r0, [r12]               /* PC исключения */
    ldr     r1, =exc_pc
    str     r0, [r1]
    ldr     r1, =exc_probe
    ldr     r1, [r1]
    cmp     r1, #0
    bne     9f
    ldr     r0, =\tag
    mov     r1, sp
    bl      panic                   /* не возвращается */
    b       .
9:  ldr     r1, =exc_caught
    mov     r2, #\tagid
    str     r2, [r1]
    add     lr, r0, #\delta         /* цель возврата — за проблемную инструкцию */
    ldmia   sp, {r0-r12}
    add     sp, sp, #64
    movs    pc, lr
.endm

EXC_HANDLER undef_handler, undef_tag, 1, 4
EXC_HANDLER svc_handler,   svc_tag,   2, 4
EXC_HANDLER pabt_handler,  pabt_tag,  3, 4
EXC_HANDLER dabt_handler,  dabt_tag,  4, 8
EXC_HANDLER irq_handler,   irq_tag,   5, 4
EXC_HANDLER fiq_handler,   fiq_tag,   6, 4
.ltorg

.section .rodata
undef_tag:  .asciz "UNDEF INSTRUCTION"
svc_tag:    .asciz "UNEXPECTED SVC"
pabt_tag:   .asciz "PREFETCH ABORT"
dabt_tag:   .asciz "DATA ABORT"
irq_tag:    .asciz "UNEXPECTED IRQ"
fiq_tag:    .asciz "UNEXPECTED FIQ"

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
.global __svc_stack_top             /* selftest в kernel_main.c сверяет SP */
__svc_stack_top:
