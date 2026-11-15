# Enclave OS — ARMv6 bare-metal ОС для Raspberry Pi 1 B+

Экспериментальная операционная система с нуля: **C + ARM-ассемблер, без libc, без Linux**.
Конечная цель — **анклавы**: изолированные пользовательские приложения, которые не могут
читать/писать память ядра, чужую память и периферию. Изоляция строится на режимах CPU,
MMU и SVC-шлюзе, а не на доверии к коду.

| | |
|---|---|
| Плата | Raspberry Pi 1 Model B+ |
| SoC | Broadcom BCM2835 |
| CPU | ARM1176JZF-S, ARMv6 (без VBAR, без аппаратного деления) |
| RAM | 512 МБ (делится с GPU) |
| Язык | C (gnu11) + ARM asm, тулчейн `arm-none-eabi` |
| Отладка | UART0 (PL011) 115200 8N1 + ACT LED (GPIO47) |
| Эмулятор | QEMU `-M raspi1ap` (тот же BCM2835/ARM1176) |
| Лицензия | см. `LICENSE` |

> **Точка входа для ИИ-агентов:** сначала `QWEN.md` (правила работы), затем этот README
> (состояние и архитектура), затем `.qwen/memory/{architecture,context,bugs}.md` (факты).
> При расхождении правил побеждает `QWEN.md`; адреса регистров — только datasheet BCM2835
> и `architecture.md`. Раздел 11 описывает цикл задачи и что делать после проверки исправлений.

---

## Содержание

1. Идея и принципы
2. Железо
3. Статус и майлстоуны (+ протокол проверки)
4. Быстрый старт: QEMU
5. Запуск на плате (SD, config.txt, UART)
6. Архитектура (boot, память, исключения, IRQ, таймер, UART, ATAG, selftest)
7. Сборка (флаги, цели make)
8. Структура проекта
9. Инварианты ядра
10. Стиль и правила кода
11. Работа с ИИ-агентами (+ чек-лист после проверки)
12. Roadmap M5–M8
13. Отладка
14. Troubleshooting
15. Шпаргалка команд
16. Материалы
17. История проекта

---

## 1. Идея и принципы

**Анклав** — пользовательский код в User mode со своим адресным пространством. Он не может
напрямую: лезть в память ядра, портить чужую память, обращаться к периферии, менять таблицы
страниц и обработчики прерываний. Единственная дверь в ядро — инструкция `SVC` (вектор 0x08).

Принципы проекта:

- **Маленькое проверяемое ядро.** Каждый майлстоун заканчивается selftest'ом и логом UART.
- **UART — единственный канал правды.** На плате нет экрана/сети; всё, что нельзя увидеть
  в UART, считается несделанным.
- **Маленький точный стек + guard page лучше большого слепого.** ARM границы стека не
  проверяет, переполнение молчаливое — поэтому стеки режимов по 4 КБ, защита придёт с MMU (M5).
- **Никакой магии в отчётах.** «Проверено» = дословный лог. «Проверено в QEMU» ≠ «проверено
  на плате» — это два разных статуса.
- **Без libc и без кучи** на текущем этапе: только свои драйверы + libgcc (деление).

## 2. Железо

- **CPU ARM1176JZF-S (ARMv6):** нет `VBAR` — векторы обязаны лежать на `0x00000000`
  (high vectors `0xFFFF0000` не используем). Нет аппаратного деления — GCC зовёт
  `__aeabi_uidivmod` из libgcc. VFP есть, но **мы его не включаем** (cp10/cp11 в CPACR),
  поэтому сборка строго soft-float.
- **Загрузка:** ARM стартует не сам. Цепочка: `Power ON -> GPU VideoCore IV -> bootcode.bin
  -> start.elf -> kernel.img @ 0x8000`. Firmware передаёт `r0=0`, `r1=machine type (0xC42)`,
  `r2=указатель ATAG (0x100)`, режим SVC, IRQ/FIQ выключены, SP — мусор.
- **ACT LED:** на B+ переехал с GPIO16 (как на модели B) на **GPIO47**, полярность прямая:
  `1 = горит`. GPIO47 не выведен на гребёнку. В QEMU светодиода нет.
- **UART0 (PL011):** GPIO14 (TX, физ. пин 8) и GPIO15 (RX, физ. пин 10), Alt0.
- **Периферия** memory-mapped от `0x20000000`; все регистры — только `volatile`.

## 3. Статус и майлстоуны

Состояние на **15.11.2026** (после восстановления M0–M2 и реализации M3/M4):

| Веха | Что | Код | QEMU | Плата |
|---|---|---|---|---|
| M0 | bootstrap: стеки всех режимов, векторы на 0x0, BSS, проброс r0/r1/r2 | ✅ | ✅ | ✅ boot/UART/LED 13.11.2026 |
| M1 | исключения: кадр r0–r12/spsr/lr/pc, panic с дампом, probe-ловушки | ✅ восстановлен 15.11 | 🔶 перепроверить | 🔶 |
| M2 | ATAG-парсер с границами + синтетические тесты | ✅ восстановлен 15.11 | 🔶 перепроверить | 🔶 |
| M3 | контроллер прерываний BCM2835 + C-диспетчер | ✅ новый 15.11 | 🔶 | ⏳ |
| M4 | тик System Timer 100 мс, heartbeat LED/UART, WFI | ✅ новый 15.11 | 🔶 | ⏳ |
| M5 | MMU: section-таблицы, device-память, guard pages | ⏳ | | |
| M6 | SVC-шлюз, User mode, `enclave_ctx` | ⏳ | | |
| M7 | round-robin на 2 анклава (M7.5 — selftest M1–M7) | ⏳ | | |
| M8 | полный прогон на плате | ⏳ | | |

Легенда: ✅ подтверждено дословным логом; 🔶 код есть, лог ожидается; ⏳ не начато.

### 3.1. Протокол проверки (после любых изменений)

```bash
make clean && make && make check && make size   # сборка + инварианты раскладки
make qemu-log                                   # 10 с прогона, лог в build/uart.log
```

Ожидание в QEMU: `RESULT: PASS` (13 passed, 0 failed; на старых QEMU без ATAG — 12 passed,
1 skipped), затем строки `alive: ...` с растущим uptime.

Плата:

```bash
make sd                          # комплект в sd/
sudo tools/make-sd.sh /dev/sdX   # разметка + заливка (деструктивно, с подтверждением)
screen /dev/ttyUSB0 115200       # снять лог
```

Ожидание на плате: вспышка LED → баннер → `RESULT: PASS` → LED мигает 1 Гц → `alive: ...`
каждую секунду.

### 3.2. Как обновлять статус

- Статус в таблице выше меняет **человек** (или memorist по его команде) только после
  дословного лога; в `.qwen/memory/context.md` memorist пишет SHA коммита и строку лога.
- Коммит — только после лога платы, сообщение по реальному диффу, в конце `QEMU: ok, плата: ok`.
- Пока проверка не пройдена, майлстоун остаётся 🔶 — не «✅ по ощущениям».

## 4. Быстрый старт: QEMU

Нужен **QEMU ≥ 6.2** (машины `raspi0`/`raspi1ap` появились в 6.2; обе — BCM2835 + ARM1176).

```bash
# Debian/Ubuntu
sudo apt install gcc-arm-none-eabi make qemu-system-arm gdb-multiarch

make            # -> build/kernel.elf + build/kernel.img
make check      # entry == 0x8000 и блок векторов < 0x100
make qemu       # UART в stdio; выход: Ctrl-A, затем X
make qemu-log   # 10 секунд, лог в build/uart.log (надёжнее для pipe/файлов)
```

**Почему ELF, а не img:** QEMU грузит ELF по физическим адресам сегментов (наш `0x8000`),
а сырой `kernel.img` кладёт на `0x10000` (`KERNEL_LOAD_ADDR`) — ядро, слинкованное на 0x8000,
там не работает. На плату, наоборот, едет именно `kernel.img`. Если очень нужен raw в QEMU:
`-device loader,file=build/kernel.img,addr=0x8000,force-raw=on` (ровно один loader!).

Ожидаемый лог (значения могут отличаться):

```
Enclave OS — ARMv6 bare metal (Raspberry Pi 1 B+)
boot: r0=0x00000000 r1=0x00000c42 r2=0x00000100
--- INFO ---
  cpu id       0x410fb767
  machine type 0x00000c42
  atags/dtb    0x00000100
  ram from tags 448 MB (2 tags)
--- SELFTEST ---
[PASS] cpu part number is ARM1176 (0xb76)
[PASS] bss zeroed by boot.S
[PASS] vector table lives at 0x00000000
[PASS] svc stack is the one boot.S set up
[PASS] undef instruction trapped and resumed
[PASS] svc trapped and resumed
[PASS] r0-r12 survive an exception
[PASS] firmware tag list walks clean
[PASS] atag synth: valid list parsed
[PASS] atag synth: oversized tag rejected
[PASS] atag synth: bad size rejected
[PASS] timer IRQ received (systimer C1)
[PASS] timer ticks advance
--- RESULT ---
selftest: 13 passed, 0 failed, 0 skipped
RESULT: PASS
--- RUN ---
heartbeat: LED 1 Hz, tick 100 ms (IRQ-driven)
alive: ticks=10 irq=10 spurious=0 uptime=1s
alive: ticks=20 irq=20 spurious=0 uptime=2s
```

Что QEMU эмулирует для `raspi1ap`:

| Устройство | В QEMU | Комментарий |
|---|---|---|
| PL011 UART0 | ✅ | `-serial stdio` / `-serial file:...` |
| System Timer (C0–C3) | ✅ | наш тик работает |
| Контроллер прерываний | ✅ | IRQ от таймера доходит до CPU |
| GPIO-регистры | ✅ | записи безвредны, **светодиода нет** |
| ATAG по 0x100 | ✅ | QEMU сам пишет CORE/MEM/NONE, r2=0x100 |
| SP804 ARM Timer | ❌ | не реализован — поэтому используем System Timer |
| Framebuffer, SD, DMA, mailbox | ✅ | нам пока не нужны |

## 5. Запуск на плате

### 5.1. SD-карта

Нужен MBR и **первый раздел FAT32**. Файлы в корне раздела:

```
bootcode.bin   start.elf   fixup.dat   config.txt   kernel.img
```

Прошивка лежит в `firmware/`. Проще всего:

```bash
make sd                          # соберёт комплект в sd/ (sd/ в .gitignore)
sudo tools/make-sd.sh /dev/sdX   # разметит и зальёт; скрипт проверяет RM=1, размер,
                                 # монтирование, требует ввести имя устройства, сверяет sha256
```

Обновление ядра на уже готовой карте:

```bash
make install SD_MNT=/media/$USER/ENCLAVE
```

### 5.2. config.txt

```
kernel=kernel.img          # имя нашего образа
kernel_address=0x8000      # явно: адрес загрузки (на Pi 1 это и так дефолт)
enable_uart=1              # UART0 на GPIO14/15
init_uart_clock=3000000    # такт UART 3 МГц — под него посчитаны делители
core_freq=250              # фиксируем core clock, чтобы такт UART не плавал
gpu_mem=16                 # framebuffer не нужен — отдаём RAM ядру (496 МБ)
```

Без `init_uart_clock` прошивка даёт другой такт, и делители IBRD=1/FBRD=40 не дают 115200
(проверено на плате 13.11: без него baud уезжает).

### 5.3. Подключение UART

USB-TTL адаптер **с уровнями 3.3 В** (5 В убьёт GPIO). Питание адаптера к плате не подключать.

| USB-TTL | Pi 1 B+ (физический пин) |
|---|---|
| GND | 6 (GND) |
| RX  | 8 (GPIO14, TX Pi) |
| TX  | 10 (GPIO15, RX Pi) |

```bash
screen /dev/ttyUSB0 115200      # или: minicom -D /dev/ttyUSB0 -b 115200
```

### 5.4. Признаки жизни и отличия от QEMU

1. Вспышка ACT LED (до инициализации UART — знак жизни, даже если UART мёртв).
2. Баннер и `--- INFO ---` в UART.
3. Selftest: на плате `ram from tags 496 MB`, число тегов больше двух (CORE/MEM/CMDLINE/...),
   ревизия MIDR может отличаться от QEMU.
4. `RESULT: PASS`, затем LED мигает 1 Гц и раз в секунду `alive: ...`.

Отличия от QEMU нормальны: объём RAM, число ATAG, ревизия CPU. Всё остальное должно совпадать.

## 6. Архитектура

### 6.1. Загрузка (boot.S)

```
_start @ 0x8000 (SVC, IRQ/FIQ off, SP=мусор)
  сохранить r0/r1/r2 firmware в r6/r5/r4 (не банкованы)
  заглушить IRQ/FIQ (trust but verify)
  стеки: IRQ -> FIQ -> Abort -> Undefined -> SVC
  скопировать векторы+пул (60 байт) на 0x00000000
  обнулить BSS (__bss_start..__bss_end)
  восстановить r0/r1/r2 и вызвать kernel_main(r0, r1, r2)
  если вернулся — вечный hang
```

### 6.2. Карта памяти

```
0x00000000  векторы исключений (копия блока [_vectors_start, _vectors_end))
0x00000100  ATAG-список от firmware (не трогать до разбора!)
0x00008000  ядро: .text .rodata .data .bss (стеки режимов — в .bss)
0x20000000  периферия BCM2835 (ARM physical; на шине GPU это 0x7E000000)
0x20003000    System Timer (1 МГц)
0x2000B200    контроллер прерываний
0x20200000    GPIO
0x20201000    UART0 PL011
0x2000B400    SP804 ARM Timer (не используем; в QEMU не реализован)
```

### 6.3. Режимы и стеки

| Режим | Стек | Назначение |
|---|---|---|
| SVC | 64 КБ (`__svc_stack_top`) | ядро |
| IRQ | 4 КБ | обработчик прерываний |
| FIQ | 4 КБ | заглушка (источников FIQ нет) |
| Abort | 4 КБ | обработчики ошибок памяти |
| Undefined | 4 КБ | обработчик undef |
| User/System | **не задаётся** | стек анклава назначит ядро при входе (M6) |

Все стеки в `.bss`, выровнены на 8 (AAPCS). Стек растёт вниз, SP ставится на конец.
До инициализации SP — ни одной инструкции, использующей стек.

### 6.4. Векторы и исключения (M1)

Таблица на 0x0: `0x00 Reset, 0x04 Undef, 0x08 SVC, 0x0C PAbt, 0x10 DAbt, 0x14 resvd,
0x18 IRQ, 0x1C FIQ`. В ячейке — инструкция `ldr pc, =handler`, пул литералов копируется
вместе с таблицей одним блоком; `_vectors_end` стоит **после** `.ltorg`.

Кадр исключения (64 байта, раскладка в `boot.S` и `struct exc_frame` в C обязана совпадать):

```
[sp+0 .. 51]  r0..r12
[sp+52]       SPSR режима
[sp+56]       LR режима (как принёс процессор)
[sp+60]       PC проблемной инструкции = LR - delta
```

| Исключение | delta | tagid |
|---|---|---|
| Undef | 4 | 1 |
| SVC | 4 | 2 |
| Prefetch Abort | 4 | 3 |
| Data Abort | 8 | 4 |
| FIQ | 4 | 6 |

(5 зарезервирован: IRQ больше не обрабатывается макросом, у него свой путь — 6.6.)

**Probe-механизм selftest:** если `exc_probe != 0`, обработчик пишет номер в `exc_caught`,
адрес проблемной инструкции в `exc_pc` и возвращается **всегда на следующую инструкцию**
(`pc + 4`, `movs pc, lr` восстанавливает CPSR из SPSR). Если `exc_probe == 0` — `panic()`
с дампом. После selftest ядро сбрасывает `exc_probe = 0`: любое неожиданное исключение = паника.

### 6.5. panic

Печатает в UART: сообщение, r0–r12, spsr, lr, pc (помечен `<- fault`), cpsr, DFAR/DFSR
(адрес и статус ошибки данных), затем `system halted` и WFI с замаскированными IRQ/FIQ.
Как искать виновника — раздел 13.

### 6.6. Прерывания (M3)

Контроллер BCM2835 @ `0x2000B200` — **не PL190/VIC**: нет VECTADDR/VICADDRESS/EOI.
Источник гасится в самом устройстве; контроллер только маскирует/показывает pending.

| Смещение | Регистр | Доступ |
|---|---|---|
| +0x00 | IRQ_basic_pending | RO |
| +0x04 | IRQ_pending1 | RO |
| +0x08 | IRQ_pending2 | RO |
| +0x0C | FIQ_control (бит 7 = enable, [6:0] = источник) | RW |
| +0x10 / +0x14 / +0x18 | Enable_IRQs1 / Enable_IRQs2 / Enable_basic | **WO** |
| +0x1C / +0x20 / +0x24 | Disable_IRQs1 / Disable_IRQs2 / Disable_basic | **WO** |

Линии: банк 1 бит 1 = System Timer C1 (наш тик), бит 3 = C3; банк 2 бит 25 = UART0
(глобальный номер 57). Enable/Disable — write-only, читать их обратно нельзя.

Путь IRQ:

```
линия IRQ -> вектор 0x18 -> irq_handler (boot.S):
  lr -= 4; сохранить r0-r12, lr (56 байт, выравнивание 8 сохраняется)
  bl irq_dispatch (interrupt.c):
    irq_count++
    pending1 & бит1 -> timer_irq_handler() (гасит CS W1C, переставляет C1, ticks++)
    остаток pending1/pending2/basic -> замаскировать + irq_spurious++
  восстановить контекст; movs pc, lr (возврат + CPSR из SPSR_irq)
```

Вложенность не разрешаем (CPSR.I взведён процессором при входе). Неожиданный источник
не роняет ядро: маскируется и попадает в счётчик `spurious` — ядро остаётся живым для
UART-лога. FIQ не используем: все источники маршрутизируются в IRQ (`FIQ_control = 0`).

### 6.7. System Timer (M4)

@ `0x20003000`, счётчик 1 МГц: `CS +0x00` (W1C биты 0–3), `CLO +0x04` (младшие 32 бита),
`CHI +0x08`, `C0 +0x0C`, `C1 +0x10`, `C2 +0x14`, `C3 +0x18`.
C0/C2 заняты GPU, для ARM свободны C1/C3 — используем **C1**.

- `timer_init(100000)`: C1 = CLO + период, снять висящий match (CS = бит 1), разрешить линию 1.
- IRQ: `CS = бит1` (write-1-to-clear — только так линия падает), `C1 += период`, `ticks++`.
- `timer_now_us()` — младшие 32 бита CLO (wrap ~71.6 мин; разности корректны через wrap).
- Главный цикл спит в **WFI** (`mcr p15,0,r0,c7,c0,4` на ARM1176) и просыпается от тика.
  Если IRQ не пришли — аварийный polling-режим по CLO с предупреждением в UART.
- Heartbeat: LED toggle каждые 5 тиков (1 Гц), лог `alive: ...` каждые 10 тиков (1 с).

### 6.8. UART PL011

@ `0x20201000`: `DR +0x00`, `FR +0x18` (TXFF бит 5, BUSY бит 3), `IBRD +0x24`, `FBRD +0x28`,
`LCRH +0x2C`, `CR +0x30`, `IMSC +0x38`, `ICR +0x44`.

Делители: `3000000 / (16 * 115200) = 1.6276` → `IBRD=1`, `FBRD=round(0.6276*64)=40`.

Последовательность init: GPIO14/15 → Alt0; `CR=0`; дождаться `!BUSY` (с таймаутом);
`ICR=0x7FF`; делители; `LCRH=0x70` (8N1 + FIFO); `IMSC=0`; `CR=UARTEN|TXE|RXE`.

- **GPPUD/GPPUDCLK не трогаем**: прошивка (`enable_uart=1`) уже настроила пины, а кривая
  последовательность GPPUDCLK вешала плату (баг 13.11, зафиксирован в bugs.md).
- `uart_putc` ждёт `!TXFF` **с таймаутом**: лучше потерять символ, чем зависнуть навсегда.
- API: `uart_init/putc/puts/puthex/putdec`; `puts` сам делает CR+LF.

### 6.9. LED

GPIO47: `GPFSEL4 +0x10` биты [23:21]=001 (output), `GPSET1 +0x20` бит 15 — включить,
`GPCLR1 +0x2C` бит 15 — выключить. Active high (подтверждено на плате).

### 6.10. ATAG-парсер (M2)

Firmware оставляет список по адресу из `r2` (обычно 0x100): `size` (в словах) → `tag` → данные.
Конец — `size==0 && tag==ATAG_NONE`. Парсер (`atag_walk`) с границами:

- выравнивание 4, адрес в RAM (< 0x20000000);
- окно поиска 64 КБ, максимум 64 тега, один тег ≤ 1024 слов;
- коды: `0` ок, `-1` size<2, `-2` size>лимита, `-3` нет NONE, `-4` выход за окно/выравнивание;
- `atag_head_ok`: первый тег ATAG_CORE с size 2 или 5;
- суммирует `ATAG_MEM` → объём RAM для INFO-блока.

Если голова не похожа на ATAG (например, DTB или мусор) — selftest ставит SKIP, не FAIL.

### 6.11. Selftest (13 проверок)

| # | Проверка | Что доказывает |
|---|---|---|
| 1 | cpu part == 0xB76 | MIDR: мы на ARM1176 |
| 2 | bss zeroed | boot.S обнулил BSS |
| 3 | vector table at 0x0 | таблица+пул скопированы дословно |
| 4 | svc stack | SP в диапазоне стека boot.S, выравнивание 8 |
| 5 | undef trapped | ловушка UDF, возврат на следующую инструкцию, exc_pc верен |
| 6 | svc trapped | SVC ловится и возвращается |
| 7 | r0–r12 survive | кадр сохраняет все 13 регистров (паттерн 0xA5A50000+n) |
| 8 | firmware ATAG | список firmware проходит парсер (SKIP, если ATAG нет) |
| 9 | synth valid | CORE+MEM(512M)+NONE разбирается |
| 10 | synth oversized | тег size=2000 отвергается (-2) |
| 11 | synth bad size | size=1 отвергается (-1) |
| 12 | timer IRQ | первый тик C1 приходит ≤ 2 с после `cpsie i` |
| 13 | ticks advance | тики продолжают идти |

Итог: `selftest: N passed, M failed, K skipped` + `RESULT: PASS/FAIL`.

## 7. Сборка

### 7.1. Флаги и зачем они

| Флаг | Зачем |
|---|---|
| `-mcpu=arm1176jzf-s` | ровно наш CPU (ARMv6), и для gcc, и для as |
| `-marm` | ARM-состояние, не Thumb (boot.S — ARM) |
| `-mfloat-abi=soft` | VFP не включён в boot.S; hard-float сгенерировал бы VFP → undef |
| `-O2 -g` | оптимизация + отладочная информация (addr2line/gdb) |
| `-ffreestanding -fno-builtin` | нет libc, нет хостовых предположений |
| `-fno-stack-protector` | нет `__stack_chk_guard`/TLS |
| `-fno-unwind-tables -fno-asynchronous-unwind-tables` | не плодить `.ARM.exidx` |
| `-nostdlib` + `-lgcc` | без libc, но **с libgcc**: на ARMv6 нет деления, нужен `__aeabi_uidivmod` |
| `-Wl,-T,linker.ld -Wl,-Map,...` | наша раскладка + карта символов |

У `as` нет опции `-marm` — только `-mcpu` (gas по умолчанию в ARM-режиме).

### 7.2. Цели make

| Цель | Действие |
|---|---|
| `all` | `build/kernel.elf` + `build/kernel.img` |
| `check` | entry == 0x8000; блок векторов < 0x100 (через readelf/nm) |
| `size` / `disasm` / `symbols` | размеры / дизассемблер в `build/kernel.dis` / nm -n |
| `qemu` / `run` | QEMU raspi1ap, UART в stdio (ELF!) |
| `qemu-log` | 10 с прогона, лог в `build/uart.log` |
| `debug` | QEMU `-s -S` + gdb-multiarch на :1234 |
| `sd` | комплект для карты в `sd/` (firmware + config.txt + kernel.img) |
| `install` | обновить kernel.img на примонтированной карте (`SD_MNT=...`) |
| `clean` | удалить `build/` |

Артефакты: `build/kernel.elf` (для QEMU/gdb), `build/kernel.img` (для платы),
`build/kernel.map`, `build/kernel.dis`, `build/uart.log`. Размер BSS ≈ 83 КБ (в основном
стеки режимов); точные числа — `make size`.

## 8. Структура проекта

```
boot.S           bootstrap, векторы, обработчики исключений, IRQ-вход, WFI, selftest-помощники
kernel_main.c    ядро: panic, ATAG, selftest, heartbeat
uart.c/h         PL011 115200 8N1 (putc/puts/puthex/putdec)
led.c/h          ACT LED GPIO47
interrupt.c/h    контроллер прерываний BCM2835 + диспетчер
timer.c/h        System Timer C1: тик 100 мс
linker.ld        раскладка @ 0x8000, ASSERT-инварианты
makefile         сборка и все цели (рецепты — табы!)
config.txt       конфиг прошивки для SD
firmware/        bootcode.bin, start.elf, fixup.dat
tools/make-sd.sh безопасная подготовка SD-карты
QWEN.md          правила работы ИИ-агентов (зона человека)
.qwen/agents/    субагенты (coder, debugger, memorist)
.qwen/commands/  команды (/code, /debug, /board, /save-session, ...)
.qwen/memory/    память проекта: architecture, context, bugs, decisions, patterns
```

## 9. Инварианты ядра (нарушать нельзя)

1. Вход `0x8000`; `_start` — первым в `.text.boot` (`KEEP` в linker.ld).
2. Блок `[_vectors_start, _vectors_end)` < `0x100` (по 0x100 лежат ATAG); обработчики —
   строго после `_vectors_end`; `.ltorg` — до `_vectors_end`.
3. SP всех режимов инициализируется до C-кода; выравнивание 8 (AAPCS); до задания SP —
   ни одной стековой инструкции.
4. `r0/r1/r2` firmware пробрасываются в `kernel_main` без потерь (через r6/r5/r4).
5. MMIO — только `volatile`; адреса регистров — только из datasheet BCM2835.
6. Возврат из исключений — только `movs/subs pc, lr` (IRQ: `lr-4`); не `bx lr`.
7. Контроллер — BCM2835, **не PL190**: нет VECTADDR/VICADDRESS/EOI; источник гасится
   в устройстве (таймер — W1C в CS); Enable/Disable — write-only.
8. Раскладка `struct exc_frame` в C == кадр в boot.S (64 байта).
9. `exc_probe/exc_caught/exc_pc` — глобальные не-static (к ним лезет ассемблер).
10. User/System стек в boot.S не задаётся — это работа ядра (M6).
11. soft-float: VFP не включён, hard-float запрещён.
12. В QEMU — ELF; на плату — raw img. Сырой img в QEMU без loader'а не грузится на 0x8000.
13. Рецепты makefile — табы; прототипы C — в заголовках (GCC 14: implicit declaration = error).
14. 0x0–0x7FFF зарезервировано (векторы/ATAG/boot-данные); ядро туда не пишет, кроме
    блока векторов на 0x0.

## 10. Стиль и правила кода

- Комментарии по-русски, отступы 4 пробела (в makefile — табы), вывод UART — ASCII.
- Регистры — `volatile uint32_t`/`unsigned int`, адреса — из datasheet BCM2835.
- Никакой стандартной библиотеки C, никакой динамической памяти.
- Глобальные переменные — только с причиной (как `exc_*` для ассемблера).
- Один майлстоун — одна задача — один коммит; сообщение по реальному диффу.
- Новый драйвер = `.c` + `.h` + строка в `C_SRC` makefile + проверка в selftest.

## 11. Работа с ИИ-агентами

Полные правила — в `QWEN.md` (его и `.qwen/agents|commands` агенты не меняют — зона человека).
Здесь — выжимка и порядок продолжения работы.

### 11.1. Роли и цикл

- **Оркестратор**: план, ревью, коммит после проверки человеком. Сам код не пишет.
- **coder**: реализует план, собирает, тестирует в QEMU.
- **debugger**: только по эскалации — анализ и план, файлы не меняет.
- **memorist**: память проекта (`/save-session`), только факты с доказательством (SHA / строка лога).

Цикл задачи: прочитать память → план (цель, файлы, инварианты, ожидаемый UART) → делегировать
coder'у одним вызовом → проверить дифф и QEMU-лог → человек проверяет на плате (`/board`) →
коммит → `/save-session`.

### 11.2. Guardrails (выстрадано, см. patterns.md / decisions.md)

- Промт coder'у — конкретные файлы, строки, старое/новое; лимит ~30% изменений файла за задачу.
- Список «НЕ ТРОГАТЬ» обязателен (boot.S, linker.ld, обработчики — без явного пункта плана).
- Один coder параллельно; 3 неудачи или `[ESCALATE]` → debugger; 2 круга без успеха → человеку.
- Проверка: `git diff` (только файлы плана) → `make clean && make` → QEMU-лог дословно.
- Запрещено всем: `sudo`, `mkfs`, `dd`, `parted`, `sfdisk`, `/dev/*`, `git push --force`,
  `reset --hard`. SD готовит человек, `git push` — только человек.
- Память писать только в `.qwen/memory/` этого проекта (не в user memory, не в QWEN.md).
- Отчёт без дословного лога — не отчёт. Коммиты «ок»/«fix» запрещены.

### 11.3. Что сделать после проверки исправлений (M0–M4)

1. Человек: `make clean && make && make check && make size` → `make qemu-log` →
   `RESULT: PASS` + растущий uptime. Сохранить `build/uart.log`.
2. Человек: `make sd` → карта → лог платы: 13 PASS, LED 1 Гц, `alive: ...`.
3. memorist: обновить `.qwen/memory/context.md` (M3/M4 завершены, SHA, строки лога) и
   `bugs.md` (закрыть: PL190-драйвер, panic-заглушка, hard-float, затирание r0 в BSS-цикле,
   probe-возврат Data Abort, опасные SD-скрипты).
4. Человек: обновить таблицу статусов в этом README (🔶 → ✅) и закоммитить:
   `M0–M4: restore M1/M2, real BCM2835 IRQ + systimer tick; QEMU: ok, плата: ok`.
5. Следующая задача — M5 по roadmap ниже, через обычный цикл.

### 11.4. Как добавлять модуль / расширять selftest

- Новый модуль: `.c` + `.h`, добавить в `C_SRC`, регистры только из datasheet, прототипы в `.h`.
- Новая проверка: `st_report(0/1/2, "имя")` до блока RESULT; probe-исключения — только
  при `exc_probe=1` и с возвратом `exc_probe=0` после.
- Ожидаемый UART-вывод формулируется в плане ДО написания кода и сверяется дословно.

### 11.5. Уроки (что уже ломали — не повторять)

| Грабли | Последствие | Правило |
|---|---|---|
| Драйвер PL190 VIC вместо BCM2835 | записи вне карты регистров, «M3» на бумаге | адреса — только из datasheet/architecture.md |
| panic = пустой while(1) | тихая смерть без дампа | panic обязан печатать кадр |
| Отчёт «добавил bl ...», которого нет в коде | фальшивая проверка | отчёт = дифф + дословный лог |
| Вырезаны selftest/ATAG при «рефакторинге» | потеря M1/M2 | удаление — только явный пункт плана |
| hard-float без включённого VFP | мина undef | soft-float, пока VFP не включён осознанно |
| r0 затёрт BSS-циклом | мусор вместо firmware-данных | проброс r0/r1/r2 через не банкованные регистры |
| mkfs без подтверждения в скрипте | риск стереть системный диск | только tools/make-sd.sh с проверками |

## 12. Roadmap M5–M8

### M5. MMU + guard pages
- Section-таблицы 1 МБ: 4096 записей × 4 байта = 16 КБ, выравнивание 16 КБ.
- Identity-map: RAM — normal/cacheable (осторожно с MMIO), периферия 0x20000000 — device
  (TEX/C/B), векторы — RO kernel-only, guard pages под стеками режимов (неприсутствующие секции).
- CP15: TTBR0 (c2), домены (c3), SCTLR (c1, бит M), инвалидация TLB перед включением.
- **Приёмка:** ядро работает с включённой MMU; запись в guard page → Data Abort с верным
  DFAR и дампом; selftest расширен проверками трансляции.

### M6. SVC-шлюз + User mode
- `struct enclave_ctx`: регистры user, SP_usr/LR_usr, состояние, точка входа.
- Вход в анклав только через exception return (в User mode `msr cpsr_c` запрещена).
- SVC-диспетчер: номера вызовов (putc, yield, exit, ...), валидация user-указателей.
- **Приёмка:** user-код печатает через SVC; прямое обращение к MMIO из User → Data Abort →
  ядро убивает анклав и остаётся живым; selftest покрывает оба сценария.

### M7. Round-robin на 2 анклава
- Переключение по тику таймера: квант N тиков, сохранение/восстановление user-контекста.
- M7.5: selftest покрывает M1–M7 (в основном образе, вывод в UART).
- **Приёмка:** два анклава печатаются вперемешку; убийство одного не влияет на второй и на ядро.

### M8. Полный прогон на плате
- Весь selftest + оба анклава на реальной Pi 1 B+, дословный лог, статусы ✅.

## 13. Отладка

```bash
make debug                 # QEMU -s -S (ждёт gdb) + gdb-multiarch на :1234
# в gdb:
#   break kernel_main
#   continue
#   info registers
#   x/10i $pc
```

По дампу panic:

```bash
arm-none-eabi-addr2line -e build/kernel.elf 0x<pc из дампа>   # файл:строка (есть -g)
make disasm                                                 # build/kernel.dis
grep -n "<pc>" build/kernel.dis                             # контекст инструкции
make symbols                                                # nm -n: где символы
```

Полезно: `objdump -h` (секции), `readelf -S` (заголовки), `kernel.map` (раскладка).
После выхода из gdb не забыть прибить фоновый QEMU (`pkill -f qemu-system-arm`).

## 14. Troubleshooting

| Симптом | Причина / решение |
|---|---|
| Нет вывода в UART (плата) | перепутаны TX/RX; не 115200; адаптер 5 В; нет GND; старый config.txt без `init_uart_clock=3000000`/`core_freq=250` |
| QEMU: `unsupported machine type raspi1ap` | QEMU < 6.2 — обновить |
| QEMU молчит с `-kernel kernel.img` | raw грузится на 0x10000; использовать ELF (`make qemu`) или один `-device loader,addr=0x8000,force-raw=on` |
| QEMU молчит при выводе в pipe | `-serial stdio` буферизуется не-TTY; `make qemu-log` (serial file) |
| LED не мигает в QEMU | нормально: GPIO-светодиод не эмулируется |
| `RESULT: FAIL` | смотреть, какая проверка упала; для panic — addr2line по pc |
| undef сразу на старте | hard-float/VFP-инструкции при выключенном VFP; проверить флаги (soft) |
| `undefined reference to __aeabi_uidivmod` | линковка без `-lgcc` |
| implicit declaration error | прототип не в заголовке (GCC 14) |
| Плата виснет после баннера | обращения вне карты регистров (привет, PL190); сверить адреса с datasheet |
| `spurious` растёт | неожиданный источник IRQ; читать pending1/2/basic, маскировать лишнее |
| Тики не идут | использован C0/C2 (заняты GPU) или не гасится CS (W1C); нужен C1/C3 |
| Плата не грузится с SD | первый раздел не FAT32/не MBR; нет bootcode.bin/start.elf/fixup.dat; kernel.img не собран |
| `*** пропущен разделитель` в make | рецепты пробелами вместо табов |

## 15. Шпаргалка команд

```bash
arm-none-eabi-as -mcpu=arm1176jzf-s boot.S -o boot.o
arm-none-eabi-gcc -mcpu=arm1176jzf-s -marm -mfloat-abi=soft -O2 -ffreestanding -c kernel_main.c
arm-none-eabi-objcopy -O binary kernel.elf kernel.img
arm-none-eabi-nm -n kernel.elf | grep -E '_start|bss|stack'
arm-none-eabi-objdump -d kernel.elf
arm-none-eabi-readelf -h kernel.elf | grep Entry
qemu-system-arm -M raspi1ap -kernel build/kernel.elf -display none -serial stdio -no-reboot
```

## 16. Материалы

- BCM2835 ARM Peripherals (datasheet, карта периферии):
  https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
- ARM1176JZF-S TRM (CP15, WFI, режимы): https://developer.arm.com/documentation/ddi0301/latest
- ARM Architecture Reference Manual, ARMv6 (DDI0100): https://developer.arm.com/documentation/ddi0100/latest
- PL011 UART TRM (DDI0183): https://developer.arm.com/documentation/ddi0183/latest
- QEMU: машины raspi: https://www.qemu.org/docs/master/system/arm/raspi.html
- Raspberry Pi config.txt: https://www.raspberrypi.com/documentation/computers/config_txt.html
- OSDev: Raspberry Pi Bare Bones: https://wiki.osdev.org/Raspberry_Pi_Bare_Bones

## 17. История проекта

- 08.2026 — старт репозитория; 11–12.11 — M0–M2 (bootstrap, исключения, ATAG, selftest).
- 13.11.2026 — первый материальный запуск на Pi 1 B+: UART подтверждён, LED на GPIO47.
- 15.11.2026 — ревизия ветки `My_Ai`: восстановлены M1/M2, настоящий M3 (BCM2835 IC) и M4
  (System Timer), исправлены сборка и boot.S, удалены опасные SD-скрипты и фальшивый отчёт.
- Учебные конспекты первых дней сохранены в истории git (старый README, коммит `35e11b3`);
  при желании их можно вынести в `docs/notes.md`.

Лицензия — см. `LICENSE`.
