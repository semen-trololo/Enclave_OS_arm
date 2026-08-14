## Конспект — День 1

### 1. Железо

- Плата: **Raspberry Pi 1 Model B+**
- SoC: **Broadcom BCM2835**
- CPU: **ARM1176JZF-S**
- Архитектура: **ARMv6**
- RAM: 512 МБ, делится между ARM и GPU
- Периферия: memory-mapped I/O

---

### 2. Карта памяти

```text
0x00000000 ... 0x1FFFFFFF   RAM для ARM
0x20000000 ... 0x20FFFFFF   периферия BCM2835
```

Важно:

```text
0x00000000 — область exception vectors / boot data
0x00008000 — сюда грузится наше ядро
0x20000000 — база периферии
```

---

### 3. Процессор ARMv6

Основные регистры:

```text
R0-R12   general purpose
R13      SP  / stack pointer
R14      LR  / link register
R15      PC  / program counter
CPSR     статус процессора и режим
```

CPSR содержит:

```text
- флаги N, Z, C, V;
- биты разрешения прерываний I, F;
- бит режима процессора;
- бит ARM/Thumb.
```

---

### 4. Режимы процессора

Основные режимы:

```text
User        непривилегированный, для приложений
Supervisor  режим ядра, в нём стартуем
IRQ         обычные прерывания
FIQ         быстрые прерывания
Abort       ошибки памяти
Undefined   неизвестные инструкции
System      привилегированный, почти как User
```

Для Enclave OS важно:

```text
User mode — анклав
Supervisor/IRQ/FIQ/Abort — ядро и обработчики
```

---

### 5. Загрузка Raspberry Pi 1

ARM стартует не сам.

Цепочка:

```text
Power ON
  -> GPU VideoCore IV
  -> bootcode.bin
  -> start.elf
  -> kernel.img
```

`start.elf` загружает наше ядро:

```text
kernel.img -> физический адрес 0x00008000
```

Затем GPU запускает ARM.

---

### 6. Состояние CPU при старте ядра

Наше ядро получает управление примерно так:

```text
PC   = 0x00008000
Mode = Supervisor
IRQ  = disabled
FIQ  = disabled
SP   = не инициализирован
```

Регистры от firmware:

```text
R0 = 0
R1 = 0xC42       machine type Raspberry Pi
R2 = указатель на ATAGS / Device Tree
```

---

### 7. Почему ядро грузится по адресу 0x8000?

Потому что начало памяти зарезервировано:

```text
0x00000000 ... 0x00007FFF   low memory reserved
0x00008000                  kernel entry
```

Там размещаются или должны размещаться:

```text
- exception vectors;
- ATAGS / Device Tree;
- служебные boot-данные.
```

Framebuffer GPU не является основной причиной.

---

### 8. Exception vectors

По умолчанию таблица векторов:

```text
0x00000000
```

Векторы:

```text
0x00  Reset
0x04  Undefined Instruction
0x08  SVC / SWI
0x0C  Prefetch Abort
0x10  Data Abort
0x14  reserved
0x18  IRQ
0x1C  FIQ
```

ARMv6 не имеет удобного `VBAR`.

Есть только:

```text
low vectors  = 0x00000000
high vectors = 0xFFFF0000
```

Перед включением прерываний мы обязаны подготовить векторы.

---

### 9. Стек

- Стек в ARM представлен регистром `R13`, то есть `SP`.
- Процессор хранит только текущий `SP`.
- Границы стека процессор сам не контролирует.
- Стек растёт вниз.
- `PUSH` сначала уменьшает `SP`, потом пишет.

Пример:

```text
SP = 0x00104000

push {r0}
SP = 0x00103FFC
[0x00103FFC] = r0

push {r1}
SP = 0x00103FF8
[0x00103FF8] = r1
```

Один регистр занимает:

```text
4 байта
```

Выравнивание стека желательно:

```text
8 байт
```

---

### 10. Главная мысль про стек

До вызова C-кода нужно обязательно задать `SP`.

Иначе:

```text
- Data Abort;
- тихое повреждение памяти;
- затирание векторов;
- затирание Device Tree;
- непредсказуемое поведение.
```

---

### 11. Стек и режимы CPU

У ARM разные режимы имеют разные `SP`.

Нужно инициализировать:

```text
SVC stack
IRQ stack
FIQ stack
Abort stack
Undefined stack
User/System stacks
```

Для ОС это критично.

---

### 12. Связь с Enclave OS

Наша ОС будет изолировать приложения через:

```text
- режимы процессора;
- MMU;
- таблицы страниц;
- exception vectors;
- системные вызовы SVC;
- защиту стеков;
- guard pages.
```

Анклав — это пользовательский код, который не может напрямую:

```text
- лезть в ядро;
- портить чужую память;
- обращаться к периферии;
- менять таблицы страниц;
- подменять обработчики прерываний.
```

---

### 13. Итог дня

```text
Raspberry Pi 1 = ARMv6 bare metal.
Сначала грузится GPU, потом ARM.
Ядро стартует в SVC mode по адресу 0x8000.
Прерывания выключены.
Стек не задан.
Low memory зарезервирована под vectors и boot data.
Перед C-кодом нужно инициализировать SP.
Перед прерываниями нужно установить exception vectors.
```
## Конспект — День 2

### 1. Задача дня
Написать `boot.S` — мост между firmware и C-кодом:
```text
сохранить boot-данные -> стеки -> векторы -> BSS -> kernel_main
```

### 2. Смена режима через CPSR
Ритуал из 4 инструкций:
```text
mrs  r0, cpsr       прочитать
bic  r0, r0, #0x1F  очистить M[4:0]
orr  r0, r0, #MODE  вставить код режима
msr  cpsr_c, r0     записать (поле _c = биты [7:0])
```
Правило: ORR только ставит биты, BIC только гасит. Поле меняется только через «погасить, потом поставить».

### 3. Банковские стеки
У каждого режима свой физический `R13` (SP). Инициализированы: IRQ, FIQ, Abort, Undefined, SVC.
```text
IRQ/FIQ/ABT/UND  по 4 КБ (1 страница)
SVC              64 КБ (boot-этап)
User/System      НЕ задаём: это стек анклава, его ставит ядро при входе
```
Логика ОС: маленький точный стек + guard page (MMU) сильнее большого слепого. ARM границы стека не проверяет — перепол молчалив.

### 4. Ловушки boot.S
```text
- FIQ банкирует R8–R12 → DTB храним в r4
- в User mode msr cpsr_c запрещена → туда только через exception return
- .align 3 = 8 байт (аргумент — степень двойки)
- до задания SP ни одной инструкции, использующей стек
```

### 5. Exception vectors
```text
0x00 Reset   0x04 Undef   0x08 SVC   0x0C PAbt
0x10 DAbt    0x14 resvd   0x18 IRQ   0x1C FIQ
```
- Ядро знает только адреса векторов (зашиты в кремний). Адреса обработчиков берёт из памяти.
- В ячейке вектора — ИНСТРУКЦИЯ, а не адрес: `ldr pc, =handler`.
- 32-битный адрес не влезает в одну инструкцию → literal pool рядом с таблицей.
- Машинный код классического вектора: `E59FF018` = `ldr pc, [pc, #0x18]`.
- PC при чтении = адрес инструкции + 8 (конвейер).
- Таблицу и пул копируем на 0x0 ОДНИМ блоком (`_vectors_end` — после `.ltorg`).

### 6. Прерывания: два уровня
```text
Ядро:      2 провода (IRQ, FIQ), векторы 0x18/0x1C, маска CPSR.I/F
Платформа: контроллер прерываний 0x2000B200, ~64 источника, свои Enable-регистры
```
- Провод IRQ — уровень (level). Контроллер держит его до гашения источника.
- Процессор НЕ знает, кто звонил: `irq_handler` сам читает pending-регистры контроллера.
- Исключения (0x04–0x10) — синхронные, от самой инструкции. Прерывания (0x18/0x1C) — асинхронные, извне.
- Закон: сначала обработчик, потом разрешение прерываний.

### 7. BSS
Глобальные переменные C без инициализатора обязаны быть нулями, в `kernel.img` их нет → обнуляем циклом `__bss_start..__bss_end`.

### 8. Вход в C-код
```text
r0 = 0, r1 = 0xC42 (machine type), r2 = DTB
bl kernel_main
после — вечный цикл hang (ядро не возвращается)
```
`linker.ld`: `. = 0x8000`, секции, символы `__bss_start/__bss_end`.

### 9. MMIO и UART
Memory-mapped I/O: периферия = регистры в адресном пространстве от `0x20000000`, доступ обычными `ldr/str`.
```text
GPIO      0x20200000   GPFSEL1: ноги 14/15 -> Alt0 (PL011)
PL011     0x20201000   DR/FR/IBRD/FBRD/LCRH/CR
115200    IBRD=1, FBRD=40 (UART clock 3 МГц)
```
Все регистры железа — только `volatile` (запрет оптимизаций компилятора).
TX: ждать `FR.TXFF==0`, писать в `DR`.

### 10. Итог дня
```text
Написан полный bootstrap: стеки всех режимов, векторы на 0x0,
BSS обнулён, kernel_main вызван, UART говорит "hello".
Инструменты дня: mrs/msr, bic/orr, ldr pc,=X, ldmia/stmia не нужны,
post-increment ldr r3,[r0],#4, objdump для проверки.
Заглушки обработчиков стоят; настоящие — в День 3.
```

### 11. Связь с Enclave OS
```text
Вектор 0x08 (SVC) — единственная дверь анклава в ядро.
Таблица векторов — святыня: позже MMU сделает её RO kernel-only.
Банки регистров изолируют РЕЖИМЫ; между анклавами SP_usr
виртуализует планировщик (struct enclave_ctx).
```
# Конспект: ассемблер + линкер. День 3

## 1. Что делает ассемблер

Ассемблер превращает код и директивы в объектный файл `.o`.

Он создаёт:

```text
секции + метки + данные + релокации
```

---

## 2. Секции

```asm
.section .text
```

код.

```asm
.section .rodata
```

константы.

```asm
.section .data
```

инициализированные данные.

```asm
.section .bss
```

неинициализированные данные.

`.bss` не занимает место в файле, но занимает место в памяти.

---

## 3. Метка

```asm
label:
```

означает:

```text
label = текущий адрес
```

Это не переменная, а символ-адрес.

Получить адрес:

```asm
ldr r0, =label
```

Прочитать память по адресу:

```asm
ldr r0, [r1]
```

---

## 4. Резервирование памяти

```asm
.byte   1        // 1 байт
.hword  0x1234   // 2 байта
.word   0x11223344 // 4 байта
.skip   4096     // пропустить 4096 байт
```

---

## 5. Выравнивание

```asm
.align 3
```

выравнивание на:

```text
2^3 = 8 байт
```

Явный вариант:

```asm
.balign 8
```

---

## 6. Пример стека

```asm
.section .bss
.align 3
irq_stack:
    .skip 4096
__irq_stack_top:
```

Память:

```text
irq_stack       -> начало
                4096 байт
__irq_stack_top -> конец
```

Стек растёт вниз, поэтому SP ставят на конец:

```asm
ldr sp, =__irq_stack_top
```

---

## 7. Что делает линкер

Линкер собирает `.o`-файлы и назначает адреса.

```text
boot.o + main.o + linker.ld -> kernel.elf
```

Линкер решает:

```text
где .text
где .data
где .bss
где __bss_start
где __bss_end
где _start
```

---

## 8. Минимальный linker script

```ld
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    . = ALIGN(8);

    .rodata : {
        *(.rodata*)
    }

    . = ALIGN(8);

    .data : {
        *(.data*)
    }

    . = ALIGN(8);

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
}
```

---

## 9. Важные строки линкера

```ld
. = 0x8000;
```

текущий адрес.

```ld
ENTRY(_start)
```

точка входа.

```ld
KEEP(*(.text.boot))
```

не выбрасывать boot-код.

```ld
__bss_start = .;
__bss_end = .;
```

создать символы начала и конца BSS.

```ld
.bss (NOLOAD)
```

BSS не хранить в файле.

---

## 10. Главное отличие

Ассемблер:

```text
создаёт секции и метки
```

Линкер:

```text
назначает им адреса
```

---

# Полезные команды

## Собрать ассемблер
```sh
arm-none-eabi-as -mcpu=arm1176jzf-s boot.S -o boot.o
```
## Слинковать

```sh
arm-none-eabi-ld -T linker.ld -nostdlib boot.o main.o -o kernel.elf
```
## Слинковать с map-файлом
```sh
arm-none-eabi-ld -T linker.ld -Map kernel.map -nostdlib boot.o main.o -o kernel.img.elf
```
## Сделать бинарник
```sh
arm-none-eabi-objcopy -O binary kernel.elf kernel.img
```
## Посмотреть символы
```sh
arm-none-eabi-nm kernel.elf
```
## Найти конкретные символы
```sh
arm-none-eabi-nm kernel.elf | grep _start
arm-none-eabi-nm kernel.elf | grep bss
arm-none-eabi-nm kernel.elf | grep stack
```
## Посмотреть секции
```sh
arm-none-eabi-objdump -h kernel.elf
```
## Посмотреть код
```sh
arm-none-eabi-objdump -d kernel.elf
```
## Посмотреть всё
```sh
arm-none-eabi-objdump -x kernel.elf
```
## Посмотреть ELF-секции
```sh
arm-none-eabi-readelf -S kernel.elf
```
## Посмотреть символы в ELF
```sh
arm-none-eabi-readelf -s kernel.elf
```
---
# Самая короткая суть

```text
Ассемблер: секции, метки, данные.
Линкер: адреса, раскладка, символы.
.bss: память есть, в файле нет, надо обнулить.
.stack: SP = top, потому что стек растёт вниз.
kernel.img: первая инструкция должна быть по адресу 0x8000.
```
