# Текущее состояние проекта

## Завершено
- [x] M0 bootstrap: стеки всех режимов, векторы на 0x0, BSS обнулён (2442a0d)
- [x] M1 обработчики исключений: кадр `r0`–`lr`, `panic` с дампом, pc = адрес инструкции
- [x] M2 проброс `r0`/`r1`/`r2` + разбор ATAG с границами (14c1f45), проверено в QEMU
- [x] Selftest в основном образе: 11 PASS / 0 FAIL / 1 SKIP, `RESULT: PASS`
- [x] Драйвер PL011 115200 8N1, linker.ld на 0x8000, makefile (run/check/debug/disasm)
- [x] Исправление критического бага r0 (f194063), constraint `+&r` (f194063)

## В планах
- [ ] M3 прерывания: VIC 0x2000B200, линия PL011 = 57, снятие источника, `CPSR.I`
- [ ] M4 тик планировщика: System Timer 0x20003000, compare-канал по IRQ
- [ ] M5 MMU + section-таблицы 1 МБ, device/nGnRE для периферии, guard page
- [ ] M6 SVC-шлюз + вход в User mode + `struct enclave_ctx`
- [ ] M7 round-robin на 2 анклава; M7.5 — selftest покрывает M1–M7
- [ ] M8 запуск на Raspberry Pi 1 Model B+ по настоящему UART

## Размеры
text 5941, data 0, bss 81976 байт
