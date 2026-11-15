# Архитектура Enclave OS (ARMv6, BCM2835) — обновлено 15.11.2026

## Железо
- Raspberry Pi 1 Model B+; BCM2835; ARM1176JZF-S (ARMv6, VBAR нет — векторы на 0x0;
  аппаратного деления нет — libgcc; VFP не включён — только soft-float). RAM 512 МБ.

## Карта памяти
- 0x00000000 векторы (копия блока [_vectors_start,_vectors_end), 60 байт)
- 0x00000100 ATAG firmware (не трогать до разбора)
- 0x00008000 ядро: .text .rodata .data .bss (стеки в .bss)
- 0x20000000 периферия (на шине GPU 0x7E000000)

## Регистры (только отсюда/datasheet)
- System Timer 0x20003000: CS +0x00 (W1C биты 0–3), CLO +0x04, CHI +0x08,
  C0 +0x0C, C1 +0x10, C2 +0x14, C3 +0x18. 1 МГц. C0/C2 — GPU, наши C1/C3.
- IC 0x2000B200 (НЕ PL190, нет VECTADDR/VICADDRESS/EOI):
  basic_pending +0x00 RO, pending1 +0x04 RO, pending2 +0x08 RO, FIQ_control +0x0C,
  Enable1/2/basic +0x10/14/18 WO, Disable1/2/basic +0x1C/20/24 WO.
  Линии: systimer C1=1, C3=3, PL011=57 (банк2 бит25).
- Mailbox 0x2000B880 (для M4.5): READ +0x00, STATUS +0x18 (бит30 EMPTY, бит31 FULL),
  WRITE +0x20; property-канал 8; буфер выровнен на 16. Теги: 0x48003 phys w/h,
  0x48004 virt w/h, 0x48009 offset, 0x48005 depth, 0x48006 order, 0x40001 alloc fb,
  0x48008 pitch. ARM-адрес fb = bus & 0x3FFFFFFF.
- GPIO 0x20200000: GPFSEL1 +0x04 (14/15 Alt0=100), GPFSEL4 +0x10 (47: [23:21]=001),
  GPSET1 +0x20 бит15, GPCLR1 +0x2C бит15. LED GPIO47 active HIGH (плата, 13.11).
- PL011 0x20201000: DR +0x00, FR +0x18 (TXFF бит5, BUSY бит3), IBRD +0x24, FBRD +0x28,
  LCRH +0x2C (0x70=8N1+FIFO), CR +0x30, IMSC +0x38, ICR +0x44.
  115200 при 3 МГц: IBRD=1 FBRD=40. GPPUD/GPPUDCLK НЕ трогать (вешало плату 13.11).
- SP804 0x2000B400 не используем: в QEMU не реализован.

## Режимы и стеки
SVC 64 КБ; IRQ/FIQ/ABT/UND по 4 КБ; User/System НЕ задаётся (стек анклава — M6).
Все в .bss, align 8 (AAPCS), SP=top (стек растёт вниз).

## Исключения
Векторы 0x00/04/08/0C/10/14/18/1C. Кадр 64 Б: r0..r12, SPSR(+52), LR(+56), PC(+60)=LR-delta.
delta: undef/svc/pabt/fiq=4, dabt=8. tagid 1,2,3,4,6 (5 зарезервирован под IRQ).
exc_probe!=0 → exc_caught=tagid, возврат на pc+4 (movs pc,lr); ==0 → panic(кадр).
IRQ отдельно: lr-4, кадр {r0-r12,lr} (56 Б), bl irq_dispatch, movs pc,lr. Вложенности нет.

## Инварианты (нарушать нельзя)
1. Вход 0x8000, _start первым (KEEP .text.boot).
2. Блок векторов <0x100; обработчики после _vectors_end; .ltorg до _vectors_end.
3. SP всех режимов до C; align 8; до SP — ни одной стековой инструкции.
4. r0/r1/r2 firmware пробрасывать без потерь (r6/r5/r4).
5. MMIO только volatile; адреса только отсюда/datasheet.
6. Возврат из исключений только movs/subs pc,lr (IRQ lr-4), не bx lr.
7. IC BCM2835: источник гасится в устройстве (таймер W1C CS); Enable/Disable WO.
8. exc_frame в C == кадр boot.S; exc_* глобальные не-static.
9. soft-float; VFP не включён.
10. QEMU — ELF (raw уехал бы на 0x10000); плата — raw img.
11. makefile: табы; прототипы в .h (GCC14 implicit declaration = error).
12. uint32_t у arm-none-eabi = unsigned long: выходные параметры uint32_t*, не unsigned int*.
13. 0x0–0x7FFF зарезервировано; пишем туда только блок векторов.

## QEMU vs плата (проверено 15.11)
- QEMU raspi1ap: ELF стартует напрямую, r0=r1=r2=0, ATAG нет → SKIP (12+1 норма).
  GPIO-светодиода нет. mailbox+fb эмулируются. SP804 нет.
- Плата: r1=0xC42; без device_tree= прошивка оставляет r2=0 (DT не найден) → SKIP;
  с device_tree= ожидаем r2=0x100 и 13 PASS (не проверено). Такт UART задаёт config.txt.
