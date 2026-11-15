/* boot.S — Enclave OS, Raspberry Pi 1 B+ (BCM2835, ARM1176JZF-S)
   M0: стеки всех режимов -> векторы на 0x0 -> BSS -> kernel_main(r0,r1,r2).
   M1: обработчики исключений с кадром r0-r12/spsr/lr/pc.
   M3: irq_handler сохраняет контекст и зовёт C-диспетчер irq_dispatch. */

.section .text.boot
.global _start
.type _start, %function
_start:
    /* ==== ВХОД: PC=0x8000, SVC, IRQ/FIQ off, SP = мусор ==== */
    mov     r6, r0                  /* r6 := r0 от firmware */
    mov     r5, r1                  /* r5 := machine type */
    mov     r4, r2                  /* r4 := ATAGS/DTB; r4-r6 не банкованы */

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

    /* ---- векторы на 0x00000000 (блок 60 байт < 0x100, ATAG не трогаем) ---- */
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

    /* ---- kernel_main(r0, r1, r2) ровно как принесла firmware ---- */
    mov     r0, r6
    mov     r1, r5
    mov     r2, r4
    bl      kernel_main

hang:                               /* ядро не должно возвращаться */
    b       hang

    .ltorg                          /* пул для всего кода выше */

/* ================= ТАБЛИЦА ВЕКТОРОВ ================= */
.align 2
.global _vectors_start
_vectors_start:
    ldr     pc, =_start             /* 0x00 Reset */
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
   Стоят после _vectors_end — иначе копирка утащила бы их на 0x0.

   Кадр на стеке режима (64 байта, порядок совпадает с
   struct exc_frame в kernel_main.c):
       [sp + 0  .. 51]  r0..r12
       [sp + 52]        SPSR режима
       [sp + 56]        LR режима как принёс процессор
       [sp + 60]        PC проблемной инструкции = LR - delta

   delta: undef/svc/pabt — LR завышен на 4; data abort — на 8.
   Пробный режим (exc_probe != 0): номер пишется в exc_caught,
   возврат ВСЕГДА на инструкцию СРАЗУ ПОСЛЕ проблемной (pc + 4),
   movs pc восстанавливает CPSR из SPSR. Без exc_probe — panic. */

.macro EXC_HANDLER label, tag, tagid, delta
.align 2
.global \label
.type \label, %function
\label:
    sub     sp, sp, #64             /* стек режима уже задан в _start */
    stmia   sp, {r0-r12}            /* сохраняем ДО того, как начнём портить r0 */
    mrs     r0, cpsr
    orr     r0, r0, #0xC0
    msr     cpsr_c, r0
    add     r12, sp, #52
    mrs     r0, spsr
    str     r0, [r12], #4           /* SPSR */
    str     lr, [r12], #4           /* LR режима */
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
    add     lr, r0, #4              /* следующая инструкция за проблемной */
    ldmia   sp, {r0-r12}
    add     sp, sp, #64
    movs    pc, lr
.endm

EXC_HANDLER undef_handler, undef_tag, 1, 4
EXC_HANDLER svc_handler,   svc_tag,   2, 4
EXC_HANDLER pabt_handler,  pabt_tag,  3, 4
EXC_HANDLER dabt_handler,  dabt_tag,  4, 8
EXC_HANDLER fiq_handler,   fiq_tag,   6, 4

/* ============ IRQ (M3): полный контекст + C-диспетчер ============
   LR_irq при входе = адрес возврата + 4. Вложенность не разрешаем:
   CPSR.I уже взведён процессором, FIQ-источников нет.
   14 регистров = 56 байт — выравнивание стека 8 сохраняется (AAPCS). */
.align 2
.global irq_handler
.type irq_handler, %function
irq_handler:
    sub     lr, lr, #4
    stmfd   sp!, {r0-r12, lr}
    bl      irq_dispatch
    ldmfd   sp!, {r0-r12, lr}
    movs    pc, lr                  /* возврат + CPSR := SPSR_irq */

/* ============ помощники selftest (M1) ============
   selftest_regs_asm(buf): забивает r0-r12 паттерном 0xA5A50000+n,
   делает svc (exc_probe должен быть 1), затем сливает все 13
   регистров в buf — C-код проверяет, что кадр их не потерял. */
.align 2
.global selftest_regs_asm
.type selftest_regs_asm, %function
selftest_regs_asm:
    stmfd   sp!, {r4-r12, lr}       /* 10 регистров = 40 байт */
    str     r0, [sp, #-8]!          /* слот под buf, выравнивание 8 держим */
    ldr     r0,  =0xA5A50000
    ldr     r1,  =0xA5A50001
    ldr     r2,  =0xA5A50002
    ldr     r3,  =0xA5A50003
    ldr     r4,  =0xA5A50004
    ldr     r5,  =0xA5A50005
    ldr     r6,  =0xA5A50006
    ldr     r7,  =0xA5A50007
    ldr     r8,  =0xA5A50008
    ldr     r9,  =0xA5A50009
    ldr     r10, =0xA5A5000A
    ldr     r11, =0xA5A5000B
    ldr     r12, =0xA5A5000C
    svc     #0
    sub     sp, sp, #64             /* площадка под дамп паттернов */
    stmia   sp, {r0-r12}
    ldr     r0, [sp, #64]           /* buf из слота */
    mov     r1, sp
    mov     r2, #13
1:  ldr     r3, [r1], #4
    str     r3, [r0], #4
    subs    r2, r2, #1
    bne     1b
    add     sp, sp, #72             /* 64 (дамп) + 8 (слот) */
    ldmfd   sp!, {r4-r12, pc}

/* WFI для ARM1176: mcr p15,0,Rd,c7,c0,4 (мнемоника wfi — только с ARMv6K+) */
.align 2
.global cpu_wfi
.type cpu_wfi, %function
cpu_wfi:
    mov     r0, #0
    mcr     p15, 0, r0, c7, c0, 4
    bx      lr

.ltorg

.section .rodata
undef_tag:  .asciz "UNDEF INSTRUCTION"
svc_tag:    .asciz "UNEXPECTED SVC"
pabt_tag:   .asciz "PREFETCH ABORT"
dabt_tag:   .asciz "DATA ABORT"
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
svc_stack:  .skip 65536             /* 64 КБ ядру */
.global __svc_stack_top             /* selftest сверяет SP */
__svc_stack_top:
