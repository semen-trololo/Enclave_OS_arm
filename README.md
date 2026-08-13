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
