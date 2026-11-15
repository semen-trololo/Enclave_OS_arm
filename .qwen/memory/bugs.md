# Баги: активные обходные пути + архив (обновлено 15.11.2026)

## АКТИВНЫЕ (обходные пути, не баги ядра)
- Плата: без device_tree= прошивка ищет DT, не находит, r2=0 → ATAG SKIP.
  Доказательство: лог «dterror: Failed to load Device Tree file '0'», «r2=0x00000000».
  Обход: device_tree= в config.txt (ожидание r2=0x100 — проверить) либо принять SKIP.
- QEMU raspi1ap: ELF стартует напрямую, r0=r1=r2=0, ATAG нет → SKIP — норма, не «чинить».
  Доказательство: лог QEMU «boot: r0=0x0 r1=0x0 r2=0x0», 12+1.
- QEMU -serial stdio в pipe теряет вывод → только -serial file: (make qemu-log).
- Плата: без init_uart_clock=3000000 прошивка уводит baud (лог «103448.300000 Hz») → кракозябры.
  Обход: config.txt всегда из репо (copy-to-sd.sh пересобирает комплект).
- tools/make-sd.sh зря удалён в 767f0bf (не был в списке удаления).
  Фикс: git checkout 2fcf7dd -- tools/make-sd.sh. Правило: удалять только по списку плана.

## ИСПРАВЛЕНО в ревизию 15.11 (код — коммит 767f0bf)
- PL190-драйвер вместо BCM2835 IC (VICVECTADDR/VICADDRESS вне карты регистров).
  Доказательство: старый interrupt.h; QWEN.md запрещал PL190. Фикс: interrupt.c/h + irq_dispatch.
- panic = пустой while(1), первое IRQ = тихая смерть. Фикс: дамп кадра + DFAR/DFSR.
- m3_fix_summary.md описывал «bl interrupt_handler», которого не было в boot.S — фальшивый
  отчёт, удалён. Правило: отчёт = дифф + дословный лог.
- makefile hard-float без включённого VFP; нет -lgcc/-O2/check. Фикс: soft-float, -lgcc, check.
- boot.S затирал r0 BSS-циклом (регрессия после f194063). Фикс: r0→r6.
- probe-возврат Data Abort перепрыгивал 2 инструкции. Фикс: возврат всегда pc+4.
- kernel_main потерял M1/M2 (кадр/ATAG/selftest). Фикс: восстановлено + 13 проверок.
- uint32_t у arm-none-eabi = unsigned long → incompatible pointer types (unsigned int*).
  Доказательство: ошибки сборки 15.11 «expected 'unsigned int *' but … 'long unsigned int *'».
  Фикс: выходные параметры uint32_t*. Правило: проверять целевым тулчейном.
- Залёжный sd/ протащил старый config.txt (972 Б) на карту → baud 103448, кракозябры.
  Доказательство: лог прошивки «Read File: config.txt, 972». Фикс: комплект всегда свежий.
- -Wmisleading-indentation (for/if без скобок). Фикс: фигурные скобки.
- check_sd.sh/full_sd_setup.sh: хардкод путей и /dev/sdc, mkfs без подтверждения — удалены.

## АРХИВ (ноябрь, до ревизии)
- 12.11 boot_r0 затирался mov r0,#0 [f194063]; constraint «r»→«+&r» [f194063].
- 13.11 GPPUDCLK в uart_init вешал плату → блок удалён; uart_putc без таймаута → таймаут;
  LED GPIO16(active low) vs GPIO47(active high) → всё на GPIO47; init_uart_clock не задавался
  → добавлен в config.txt; двойной -device loader глушил ядро → loader ровно один;
  raw -kernel не на 0x8000 → ELF для QEMU; табы в makefile; ATAG tag читается со +4, не +0.
