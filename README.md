# Enclave OS — ARMv6 bare-metal ОС для Raspberry Pi 1 B+

Экспериментальная ОС с нуля: **C + ARM-ассемблер, без libc, без Linux**.
Цель — **анклавы**: изолированные пользовательские приложения (MMU + User mode + SVC-шлюз).
Каналы вывода: UART0 (основной лог) и — с вехи M4.5 — framebuffer на монитор.

| | |
|---|---|
| Плата | Raspberry Pi 1 Model B+ |
| SoC / CPU | Broadcom BCM2835 / ARM1176JZF-S (ARMv6, без VBAR, без аппаратного деления) |
| RAM | 512 МБ (делится с GPU; gpu_mem=16 → ядру 496 МБ) |
| Язык | C (gnu11) + ARM asm, тулчейн `arm-none-eabi` |
| Отладка | UART0 PL011 115200 8N1 + ACT LED (GPIO47) |
| Эмулятор | QEMU ≥ 6.2, `-M raspi1ap` |
| Лицензия | см. `LICENSE` |

> **Точка входа для ИИ-агентов:** `QWEN.md` (правила) → этот README (состояние) →
> `.qwen/memory/{architecture,context,bugs}.md` (факты). При расхождении правил побеждает
> `QWEN.md`; адреса регистров — только datasheet BCM2835 и `architecture.md`.

## Содержание
1. Идея и принципы
2. Железо
3. Статус и майлстоуны (+ протокол проверки)
4. Быстрый старт: QEMU
5. Запуск на плате (SD, config.txt, UART, консоль)
6. Архитектура
7. Сборка
8. Структура проекта
9. Инварианты ядра
10. Стиль и правила кода
11. Работа с ИИ-агентами (CLI)
12. Roadmap M4.5–M8
13. Отладка
14. Troubleshooting
15. Шпаргалка команд
16. Материалы
17. История проекта

## 1. Идея и принципы

**Анклав** — код в User mode со своим адресным пространством; не может лезть в память ядра,
чужую память и периферию. Единственная дверь в ядро — `SVC` (вектор 0x08).

- Маленькое проверяемое ядро: каждая веха заканчивается selftest'ом и логом UART.
- UART — канал правды; framebuffer (M4.5) — второй канал, не замена.
- Маленький точный стек + guard page лучше большого слепого (защита придёт с MMU, M5).
- «Проверено» = дословный лог. «Проверено в QEMU» ≠ «проверено на плате».
- Без libc и без кучи: свои драйверы + libgcc (деление).

## 2. Железо

- ARMv6: нет VBAR → векторы только на 0x0; нет аппаратного деления → libgcc;
  VFP есть, но не включён → сборка строго soft-float.
- Загрузка: `GPU -> bootcode.bin -> start.elf -> kernel.img @ 0x8000`.
  Firmware: r0=0, r1=machine type, r2=ATAG/DTB; режим SVC, IRQ/FIQ off, SP — мусор.
- ACT LED: GPIO47, active high (1 = горит), на гребёнку не выведен.
- UART0: GPIO14 (TX, пин 8), GPIO15 (RX, пин 10), Alt0.
- Периферия от 0x20000000, все регистры только `volatile`.

## 3. Статус и майлстоуны

Состояние на **15.11.2026**. Код ревизии — коммит `767f0bf` (запушен).
Проверено и в QEMU, и на плате; логи платы в репо: `logs/board-20261115-*.log`.

| Веха | Что | Код | QEMU | Плата |
|---|---|---|---|---|
| M0 | bootstrap: стеки, векторы на 0x0, BSS, проброс r0/r1/r2 | ✅ | ✅ | ✅ |
| M1 | исключения: кадр r0–r12/spsr/lr/pc, panic с дампом, probe-ловушки | ✅ | ✅ | ✅ |
| M2 | ATAG-парсер с границами + синтетика | ✅ | ✅ | ✅ (firmware-теги: SKIP, см. 3.2) |
| M3 | контроллер прерываний BCM2835 + C-диспетчер | ✅ | ✅ | ✅ |
| M4 | тик System Timer 100 мс, heartbeat LED/UART, WFI | ✅ | ✅ | ✅ (LED — визуально подтвердить) |
| M4.5 | framebuffer: mailbox + fb-консоль на монитор | ⏳ план | | |
| M5 | MMU: section-таблицы, device-память, guard pages | ⏳ | | |
| M6 | SVC-шлюз, User mode, `enclave_ctx` | ⏳ | | |
| M7 | round-robin на 2 анклава (M7.5 — selftest M1–M7) | ⏳ | | |
| M8 | полный прогон на плате | ⏳ | | |

Доказательства (дословно):
- QEMU: `selftest: 12 passed, 0 failed, 1 skipped` / `RESULT: PASS`, `alive: ticks=… irq=… spurious=0`.
- Плата: `boot: r0=0x00000000 r1=0x00000c42 r2=0x00000000`, `selftest: 12 passed, 0 failed, 1 skipped`,
  `RESULT: PASS`, `alive: ticks=84 irq=84 spurious=0 uptime=8s`.
- Прошивка: `Loaded 'kernel.img' to 0x8000 size 0x1831` (6193 байта).

### 3.1. Протокол проверки (после любых изменений)

```bash
make clean && make && make check && make size
make qemu-log        # 10 с, лог в build/uart.log; ждём RESULT: PASS и растущий uptime
```

Плата:

```bash
tools/copy-to-sd.sh /dev/sdX          # комплект всегда собирается свежим
tools/board-console.sh /dev/ttyUSB0   # запустить ДО подачи питания
# переткнуть питание / Reset; лог пишется в logs/board-<дата>.log
```

### 3.2. Известные особенности стендов (не баги ядра)

- **QEMU raspi1ap**: наш ELF запускается напрямую с точки входа, без бут-стаба →
  `r0=r1=r2=0`, ATAG нет → проверка firmware-тегов = SKIP (12+1 — норма).
- **Плата, эта прошивка**: без `device_tree=` в config.txt пытается грузить DT, не находит
  и оставляет `r2=0` → тоже SKIP. Опция `device_tree=` (пустое значение) должна включить
  режим ATAG (r2=0x100, 13 PASS) — **ещё не проверено на плате**.
- **gpu_mem=16** в новом config.txt: проверенный прогон платы был с дефолтным gpu_mem;
  с 16 ядру достанется 496 МБ — selftest проверяет только `mem>0`, не сломается.
- Светодиода в QEMU нет; LED 1 Гц — только на плате (led.c не менялся с 13.11, тогда подтверждён).

### 3.3. Как обновлять статус

Только после дословного лога; memorist пишет в `context.md` SHA + строку лога;
коммит — после лога платы, в конце сообщения `QEMU: ok, плата: ok`.

## 4. Быстрый старт: QEMU

```bash
sudo apt install gcc-arm-none-eabi make qemu-system-arm gdb-multiarch   # Debian/Ubuntu
make && make check
make qemu        # UART в stdio; выход Ctrl-A, X
make qemu-log    # 10 с, лог в build/uart.log
```

**ELF vs img:** QEMU грузит ELF по адресам сегментов (0x8000); сырой img положил бы на
0x10000. В QEMU — `build/kernel.elf`, на плату — `build/kernel.img`.

Что эмулирует raspi1ap: PL011 ✅, System Timer ✅, контроллер прерываний ✅, GPIO-регистры ✅
(светодиода нет), mailbox property ✅, framebuffer ✅ (пригодится для M4.5), SP804 ❌.
ATAG в нашей конфигурации QEMU не передаёт (ELF стартует напрямую) — SKIP норма.

## 5. Запуск на плате

### 5.1. SD-карта

Первая подготовка (один раз, деструктивно, с подтверждениями):

```bash
sudo tools/make-sd.sh /dev/sdX
```

Дальше — только безопасное обновление (ничего не форматирует):

```bash
tools/copy-to-sd.sh /dev/sdX            # или точку монтирования: /media/$USER/boot
```

Скрипт: проверяет съёмность (RM=1) и размер, работает с уже примонтированным разделом
(авто-монт Debian), **всегда пересобирает комплект** из `firmware/ + config.txt +
build/kernel.img` (залёжный `sd/` больше не протащит старый config), сверяет каждый файл
по sha256, размонтирует только то, что смонтировал сам.

Файлы на карте (FAT32, первый раздел MBR): `bootcode.bin start.elf fixup.dat config.txt kernel.img`.

### 5.2. config.txt

```
kernel=kernel.img
kernel_address=0x8000
enable_uart=1
init_uart_clock=3000000
core_freq=250
gpu_mem=16
device_tree=
```

- `init_uart_clock=3000000` + `core_freq=250` — такт UART под делители IBRD=1/FBRD=40.
  Без них прошивка уводит baud (наблюдали 103448 Гц → кракозябры).
- `gpu_mem=16` — максимум RAM ядру. **Для M4.5 (framebuffer) поднять до 64.**
- `device_tree=` — пустое значение: режим ATAG вместо DT (ожидаем r2=0x100; проверить).

### 5.3. UART и консоль

USB-TTL **3.3 В** (5 В убьёт GPIO), питание адаптера не подключать:

| USB-TTL | Pi 1 B+ (физ. пин) |
|---|---|
| GND | 6 |
| RX  | 8 (GPIO14, TX Pi) |
| TX  | 10 (GPIO15, RX Pi) |

```bash
tools/board-console.sh /dev/ttyUSB0   # 115200 8N1, экран + logs/board-<дата>.log
```

Признаки жизни: вспышка LED → баннер → selftest → `RESULT: PASS` → LED 1 Гц + `alive: …` раз в секунду.

## 6. Архитектура

### 6.1. Загрузка (boot.S)
```
_start @ 0x8000: r0/r1/r2 -> r6/r5/r4 -> стеки IRQ/FIQ/ABT/UND/SVC
  -> векторы+пул (60 байт) на 0x0 -> BSS -> kernel_main(r0,r1,r2) -> hang
```

### 6.2. Карта памяти
```
0x00000000 векторы | 0x00000100 ATAG | 0x00008000 ядро (.text/.rodata/.data/.bss+стеки)
0x20000000 периферия: 0x20003000 System Timer, 0x2000B200 IC, 0x2000B880 mailbox,
0x20200000 GPIO, 0x20201000 UART0. SP804 0x2000B400 не используем (нет в QEMU).
```

### 6.3. Стеки
SVC 64 КБ; IRQ/FIQ/ABT/UND по 4 КБ; User/System не задаётся (стек анклава — M6).
Все в .bss, выравнивание 8 (AAPCS).

### 6.4. Исключения (M1)
Кадр 64 байта: `[sp+0..51] r0..r12, [sp+52] SPSR, [sp+56] LR, [sp+60] PC=LR-delta`.
delta: undef/svc/pabt/fiq=4, dabt=8. tagid: 1..4,6 (5 зарезервирован).
`exc_probe!=0` → номер в `exc_caught`, возврат всегда на `pc+4` (`movs pc,lr`);
`exc_probe==0` → panic с дампом (r0–r12, spsr, lr, pc, cpsr, DFAR, DFSR).

### 6.5. Прерывания (M3)
BCM2835 IC @ 0x2000B200 — **не PL190**: pending RO (+0x00/04/08), FIQ_control +0x0C,
Enable WO (+0x10/14/18), Disable WO (+0x1C/20/24). Линии: C1=1, C3=3, UART0=57.
Путь: вектор 0x18 → irq_handler (lr-4, кадр 14 регистров) → irq_dispatch → таймер /
маскировка неизвестных (`spurious`). Возврат `movs pc,lr`. Вложенности нет.

### 6.6. System Timer (M4)
@0x20003000, 1 МГц: CS +0x00 (W1C), CLO +0x04, C1 +0x10. C0/C2 заняты GPU.
Тик 100 мс: CS=бит1, C1+=период, ticks++. Главный цикл — WFI; LED каждые 5 тиков,
лог каждые 10. Нет IRQ → polling-fallback по CLO с предупреждением.

### 6.7. UART
PL011 @0x20201000: DR/FR/IBRD/FBRD/LCRH/CR/IMSC/ICR. 3 МГц/(16·115200)=1.6276 → 1/40.
Init: Alt0 → CR=0 → !BUSY (таймаут) → ICR=0x7FF → делители → LCRH=0x70 → IMSC=0 →
CR=UARTEN|TXE|RXE. GPPUD/GPPUDCLK не трогаем (вешало плату). putc — с таймаутом.

### 6.8. ATAG (M2)
`size(слова) → tag → данные`, конец size==0&&tag==0. Границы: окно 64 КБ, ≤64 тегов,
тег ≤1024 слов, выравнивание 4, адрес в RAM. Коды: 0/-1/-2/-3/-4. Не похоже на ATAG → SKIP.

### 6.9. Selftest (13 проверок)
cpu 0xB76; bss; векторы на 0x0; svc-стек+выравнивание; undef-ловушка; svc-ловушка;
r0–r12 survive; firmware ATAG (или SKIP); синтетика valid/oversized/bad-size;
первый IRQ таймера ≤2 с; тики идут. Итог `RESULT: PASS/FAIL`.

## 7. Сборка

Флаги: `-mcpu=arm1176jzf-s -marm -mfloat-abi=soft -O2 -g -Wall -Wextra -std=gnu11
-ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables
-fno-asynchronous-unwind-tables`; линковка `-nostdlib … -lgcc` (деление ARMv6).
У `as` нет `-marm` — только `-mcpu`. Зависимости от заголовков есть (`-MMD -MP`).

Цели: `all check size disasm symbols qemu run qemu-log debug sd install clean`.
`check`: entry==0x8000 и блок векторов <0x100. Артефакты в `build/`.

## 8. Структура проекта

```
boot.S kernel_main.c uart.c/h led.c/h interrupt.c/h timer.c/h linker.ld makefile config.txt
firmware/               bootcode.bin start.elf fixup.dat
tools/make-sd.sh        первая подготовка SD (деструктивно, с проверками)
tools/copy-to-sd.sh     безопасное обновление SD (всегда свежий комплект, sha256)
tools/board-console.sh  UART-консоль платы с логом в logs/
logs/                   логи платы (доказательства; новые — в .gitignore)
QWEN.md                 правила агентов (зона человека)
.qwen/agents/           coder, debugger, memorist (model обязателен!)
.qwen/commands/         /answer /code /debug /board /save-session
.qwen/memory/           architecture, context, bugs, decisions, patterns, delegate-to-coder
```

## 9. Инварианты ядра (нарушать нельзя)

1. Вход 0x8000; `_start` первым в `.text.boot` (KEEP).
2. Блок векторов <0x100 (ATAG по 0x100); обработчики после `_vectors_end`; `.ltorg` до него.
3. SP всех режимов до C-кода; выравнивание 8; до задания SP — ни одной стековой инструкции.
4. r0/r1/r2 firmware — без потерь (r6/r5/r4).
5. MMIO только volatile; адреса — только datasheet/architecture.md.
6. Возврат из исключений только movs/subs pc,lr (IRQ: lr-4).
7. IC — BCM2835, не PL190; источник гасится в устройстве (таймер — W1C CS); Enable/Disable WO.
8. `struct exc_frame` == кадр boot.S (64 байта).
9. `exc_probe/exc_caught/exc_pc` — глобальные не-static.
10. User/System стек не задаётся в boot.S (M6).
11. soft-float, пока VFP не включён осознанно.
12. QEMU — ELF; плата — raw img.
13. Рецепты makefile — табы; прототипы — в заголовках.
14. 0x0–0x7FFF зарезервировано; пишем туда только блок векторов.
15. **uint32_t у arm-none-eabi = unsigned long**: выходные параметры — `uint32_t*`, не `unsigned int*`.

## 10. Стиль и правила кода

Комментарии по-русски, 4 пробела (makefile — табы), UART — ASCII. Без libc и динамики.
Глобальные — только с причиной. Один майлстоун — одна задача — один коммит.
Новый драйвер = .c + .h + строка в C_SRC + проверка в selftest.
Удаление файлов — только по явному списку в плане (грабли: make-sd.sh удалён зря в 767f0bf).

## 11. Работа с ИИ-агентами (CLI)

Правила — `QWEN.md`. Роли: оркестратор (план/ревью/коммит), coder (реализация+QEMU),
debugger (эскалации, файлы не меняет), memorist (память, факты с доказательством).

Цикл: память → план → /code (один coder за раз) → ревью диффа и QEMU-лога →
человек проверяет на плате (/board) → коммит → /save-session.

Особенности CLI-окружения:
- В каждом `.qwen/agents/*.md` **обязательно поле `model`**, ID точно из каталога провайдера,
  без выдуманных префиксов (грабли: `qwen/qwen/…` — двойной префикс, субагент не стартует).
- Guardrails: конкретные файлы/строки в промте, лимит ~30% файла за задачу, список
  «НЕ ТРОГАТЬ», 3 неудачи → [ESCALATE] → debugger; 2 круга без успеха → человеку.
- Запрещено всем: sudo, mkfs, dd, parted, sfdisk, /dev/*, push --force, reset --hard.
- Память — только `.qwen/memory/` этого проекта; пишет только memorist.
- Отчёт без дословного лога — не отчёт.

## 12. Roadmap

### M4.5. Framebuffer + консоль на монитор (следующая по желанию человека)
- Mailbox property: ARM mailbox 0 @ **0x2000B880** (READ +0x00, STATUS +0x18:
  бит30 EMPTY / бит31 FULL, WRITE +0x20), канал **8**. Буфер 16-байтно выровнен.
- Теги: 0x48003 physical w/h, 0x48004 virtual w/h, 0x48009 offset, 0x48005 depth=32,
  0x48006 pixel order, **0x40001 allocate fb** (align 16) → bus-адрес+размер,
  0x48008 get pitch, конец 0. ARM-адрес = bus & 0x3FFFFFFF (алиасы уточнить по ответу).
- config.txt: `gpu_mem=64`, `hdmi_force_hotplug=1`, для первого света 640x480@60
  (`hdmi_group=1`, `hdmi_mode=1`). RAM ядра станет 448 МБ — selftest это переживает.
- Драйвер fb + шрифт 8x16 + fb_puts; UART остаётся основным логом.
- Selftest: mailbox ответил; fb addr≠0; pitch≥w*4; запись/чтение пикселя.
- **QEMU эмулирует mailbox+fb** → почти всё проверяется в эмуляторе; монитор — на плате.
- После M5: fb маппить device/non-cacheable.
- Файлы: mailbox.c/h, fb.c/h, font8x16.*, kernel_main.c, makefile, config.txt.

### M5. MMU + guard pages
Section-таблицы 1 МБ (16 КБ, выравнивание 16 КБ), identity-map, периферия device,
векторы RO kernel-only, guard pages под стеками; CP15 TTBR0/домены/SCTLR, инвалидация TLB.
Приёмка: ядро с MMU; запись в guard → Data Abort с верным DFAR; selftest расширен.

### M6. SVC-шлюз + User mode
`enclave_ctx`, вход через exception return, SVC-диспетчер (putc/yield/exit), валидация
user-указателей. Приёмка: user печатает через SVC; MMIO из User → abort → анклав убит, ядро живо.

### M7. Round-robin на 2 анклава
Переключение по тику, квант N тиков, сохранение user-контекста; M7.5 — selftest M1–M7.
Приёмка: два анклава вперемешку; убийство одного не влияет на второй.

### M8. Полный прогон на плате
Весь selftest + анклавы на реальной Pi 1 B+, дословный лог.

## 13. Отладка

```bash
make debug    # QEMU -s -S + gdb-multiarch :1234 (break kernel_main; x/10i $pc)
arm-none-eabi-addr2line -e build/kernel.elf 0x<pc из panic>
make disasm && grep -n "<pc>" build/kernel.dis
make symbols
```
После gdb прибить фоновый QEMU: `pkill -f qemu-system-arm`.

## 14. Troubleshooting

| Симптом | Причина / решение |
|---|---|
| Кракозябры после «Starting ARM» | на карте чужой/старый config.txt (нет init_uart_clock) → copy-to-sd.sh всегда освежает комплект; сверь `cat …/config.txt` |
| Прошивка: baud 103448 вместо ~115384 | не применился init_uart_clock=3000000 |
| r2=0 на плате | эта прошивка без device_tree= не собирает ATAG; SKIP — норма; попробуй device_tree= |
| QEMU: r0=r1=r2=0, ATAG SKIP | ELF запускается напрямую без стаба — норма |
| QEMU молчит с raw img | raw грузится на 0x10000; используй ELF |
| QEMU молчит в pipe | -serial stdio буферизуется; make qemu-log |
| LED не мигает в QEMU | светодиода там нет |
| `undefined reference to __aeabi_uidivmod` | линковка без -lgcc |
| incompatible pointer types (uint32_t*) | uint32_t = unsigned long; выходные параметры — uint32_t* |
| implicit declaration | прототип не в заголовке (GCC 14) |
| плата виснет после баннера | адреса вне карты регистров (привет, PL190) |
| spurious растёт | неожиданный источник; читать pending, маскировать |
| тики не идут | C0/C2 заняты GPU; нужен C1/C3 + W1C CS |
| SD не грузится | не FAT32/не первый раздел MBR; нет файлов прошивки |
| `*** пропущен разделитель` | рецепты makefile пробелами |
| субагент не стартует в CLI | нет поля model или ID не из каталога провайдера |

## 15. Шпаргалка команд

```bash
arm-none-eabi-as -mcpu=arm1176jzf-s boot.S -o boot.o
arm-none-eabi-objcopy -O binary kernel.elf kernel.img
arm-none-eabi-nm -n kernel.elf | grep -E '_start|bss|stack'
arm-none-eabi-readelf -h kernel.elf | grep Entry
qemu-system-arm -M raspi1ap -kernel build/kernel.elf -display none -serial stdio -no-reboot
```

## 16. Материалы

- BCM2835 ARM Peripherals: https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
- ARM1176JZF-S TRM: https://developer.arm.com/documentation/ddi0301/latest
- ARMv6 ARM (DDI0100): https://developer.arm.com/documentation/ddi0100/latest
- PL011 TRM (DDI0183): https://developer.arm.com/documentation/ddi0183/latest
- QEMU raspi: https://www.qemu.org/docs/master/system/arm/raspi.html
- config.txt: https://www.raspberrypi.com/documentation/computers/config_txt.html
- Mailbox property interface: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

## 17. История проекта

- 08.2026 — старт; 11–12.11 — M0–M2; 13.11 — первый запуск на плате (UART/LED).
- 15.11 — ревизия: восстановлены M1/M2, настоящие M3/M4, сборка/скрипты/доки;
  проверено в QEMU и на плате (12 PASS + 1 SKIP, RESULT: PASS). Код — коммит 767f0bf.
  В 767f0bf зря удалён tools/make-sd.sh — восстановлен из 2fcf7dd вторым коммитом.
- Конспекты первых дней — в истории git (старый README, коммит 35e11b3).

Лицензия — см. `LICENSE`.
