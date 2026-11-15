# Полное руководство по запуску Enclave OS на плате Raspberry Pi 1 B+

## Краткое руководство по установке

### 1. Установка на SD-карту
```bash
# Создание SD-карты (одноразово, форматирует раздел):
sudo tools/make-sd.sh /dev/sdX

# Обновление kernel.img (безопасно, не форматирует):
sudo tools/copy-to-sd.sh /dev/sdX
```

### 2. Запуск на плате
1. Вставьте SD-карту в Raspberry Pi 1 B+
2. Подключите USB-TTL адаптер:
   - GND платы (ножка 6) → GND адаптера
   - GPIO14 (TX, ножка 8) → RX адаптера
   - GPIO15 (RX, ножка 10) → TX адаптера
3. Подключите адаптер к компьютеру (USB)
4. Запустите монитор порта: `cat /dev/ttyUSB0` (115200 8N1)
5. Подайте питание на плату

## Ожидаемое поведение

При нормальном запуске на UART должен появиться следующий лог:
```
Enclave OS — ARMv6 bare metal (Raspberry Pi 1 B+)
boot: r0=0x... r1=0x... r2=0x...
--- INFO ---
  cpu id       0x410fb767
  machine type 0x...
  atags/dtb    0x...
--- SELFTEST ---
[PASS] cpu part number is ARM1176 (0xb76)
[PASS] bss zeroed by boot.S
[PASS] vector table lives at 0x00000000
[PASS] svc stack is the one boot.S set up
[PASS] undef instruction trapped and resumed
[PASS] svc trapped and resumed
[PASS] r0-r12 survive an exception
[SKIP] firmware tag list walks clean (no ATAGS)
[PASS] atag synth: valid list parsed
[PASS] atag synth: oversized tag rejected
[PASS] atag synth: bad size rejected
[PASS] timer IRQ received (systimer C1)
[PASS] timer ticks advance
[PASS] mmu guard page blocks write access
--- RESULT ---
selftest: 13 passed, 0 failed, 1 skipped
RESULT: PASS
--- RUN ---
heartbeat: LED 1 Hz, tick 100 ms (IRQ-driven)
alive: ticks=... irq=... spurious=0 uptime=...s
```

## Индикаторы работы LED

Зеленый LED (GPIO47) мигает 1 раз в секунду (heartbeat) — подтверждение нормальной работы ядра после прохождения selftest.

## Возможные проблемы

### 1. Нет вывода в UART (тишина)
- Проверьте подключение USB-TTL адаптера: GND общая, TX ↔ RX, RX ↔ TX
- Проверьте config.txt на SD-карте: должны быть строки `init_uart_clock=3000000` и `core_freq=250`
- Проверьте скорость порта (должна быть 115200 8N1)

### 2. Циклическое мигание 4 раза без UART-лога
- Это означает зависание после `uart_init()`, но до или во время первого `uart_puts()`
- Проблема может быть в аппаратной части (повреждение GPIO, проблемы с питанием)
- Или в программной части (некорректная конфигурация периферии после инициализации MMU)

### 3. Мигает LED, но вывод в UART неполный
- Возможно проблема с подключением или скоростью (baud rate)
- Попробуйте изменить скорость на 57600 или 9600
- Проверьте целостность проводов

## Дополнительные проверки

- Размер образа: `kernel.img` должен быть не более 8 МБ (по умолчанию `linker.ld` с `ASSERT`)
- Контрольная сумма: убедитесь, что `build/kernel.img` совпадает с файлом на SD-карте
  ```bash
  cmp build/kernel.img /media/enclave/boot/kernel.img
  ```

## Проверка в QEMU

```bash
make clean && make
make check          # Проверка entry point и векторов
make qemu-log       # Запуск на 10 секунд, лог в build/uart.log
```

Ожидаемый результат: **RESULT: PASS**, 13 passed, 0 failed, 1 skipped (ATAG SKIP — норма для QEMU).

## Создание SD-карты с нуля (один раз)

```bash
# Форматирование (удалит все данные!)
sudo tools/full_sd_setup.sh /dev/sdX

# Копирование новых файлов (безопасно, без форматирования)
sudo tools/copy-to-sd.sh /dev/sdX
```

После этого можно подключать плату к UART и питанию.