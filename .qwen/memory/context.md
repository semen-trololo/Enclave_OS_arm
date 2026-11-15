# Текущее состояние — 15.11.2026

## Завершено (ревизия 15.11; код в коммите 767f0bf, запушен)
- [x] M0 bootstrap: стеки, векторы 60 Б на 0x0, BSS, проброс r0/r1/r2 (r6/r5/r4).
- [x] M1 исключения: кадр 64 Б, panic с дампом (r0–r12/spsr/lr/pc/cpsr/DFAR/DFSR),
      probe-возврат pc+4; selftest_regs_asm проверяет r0–r12.
- [x] M2 ATAG: парсер с границами (окно 64К, ≤64 тегов, ≤1024 слов), 3 синтетики.
- [x] M3 IC BCM2835 (НЕ PL190): irq_handler→irq_dispatch, неизвестные → mask+spurious.
- [x] M4 System Timer C1, тик 100 мс, WFI, LED 1 Гц, alive-лог 1 Гц, polling-fallback.
- [x] Сборка: arm1176jzf-s soft-float -O2, -lgcc, -MMD -MP; check: entry 0x8000, векторы<0x100.
- [x] Инструменты: tools/copy-to-sd.sh (всегда свежий комплект, sha256),
      tools/board-console.sh (лог в logs/), make sd/install/qemu-log/debug.
- [x] Удалены: check_sd.sh, full_sd_setup.sh (опасные), m3_fix_summary.md (фальшивый отчёт).

## Доказательства
- QEMU: «selftest: 12 passed, 0 failed, 1 skipped», «RESULT: PASS», alive irq==ticks spurious=0.
- Плата (logs/board-20261115-*.log в репо): «boot: r0=0x00000000 r1=0x00000c42 r2=0x00000000»,
  «selftest: 12 passed, 0 failed, 1 skipped», «RESULT: PASS»,
  «alive: ticks=84 irq=84 spurious=0 uptime=8s».
- Прошивка: «Loaded 'kernel.img' to 0x8000 size 0x1831» (6193 Б).
- SKIP на обоих стендах — firmware-ATAG: QEMU не передаёт; плата без device_tree= даёт r2=0.
- LED 1 Гц: led.c не менялся с 13.11 (тогда подтверждён); визуальное подтверждение — при
  следующем /board.

## Размеры
kernel.img 6193 Б (0x1831); bss ≈ 83 КБ (стеки 80 КБ + буферы). Точные — make size.

## Открытые хвосты ревизии
- tools/make-sd.sh зря удалён в 767f0bf → восстановить: git checkout 2fcf7dd -- tools/make-sd.sh.
- config.txt: добавить gpu_mem=16 и device_tree= (проверенный прогон был без них;
  с gpu_mem=16 ожидаем 496 МБ, с device_tree= ожидаем r2=0x100 и 13 PASS — проверить).
- README/память/coder.md — второй коммит (документация ревизии).

## В планах (порядок выбирает человек)
- [ ] M4.5 framebuffer: mailbox 0x2000B880 канал 8, теги alloc/pitch, fb-консоль 8x16;
      config gpu_mem=64 + hdmi_force_hotplug=1; QEMU эмулирует mailbox+fb.
- [ ] M5 MMU: section 1 МБ, device-память, guard pages; приёмка — abort по guard с верным DFAR.
- [ ] M6 SVC-шлюз + User mode + enclave_ctx; M7 round-robin (M7.5 selftest M1–M7); M8 плата.

## Протокол проверки
make clean && make && make check && make size → make qemu-log (RESULT: PASS, uptime растёт)
→ tools/copy-to-sd.sh /dev/sdX → tools/board-console.sh → лог платы → коммит → /save-session.
