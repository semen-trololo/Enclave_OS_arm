# Makefile — Enclave OS, Raspberry Pi 1 B+ (BCM2835, ARM1176JZF-S)

# Кросс-компилятор
CROSS_COMPILE := arm-none-eabi-
CC            := $(CROSS_COMPILE)gcc
AS            := $(CROSS_COMPILE)as
LD            := $(CROSS_COMPILE)ld
OBJCOPY       := $(CROSS_COMPILE)objcopy

# Папки
BUILD_DIR     := build
TARGET        := $(BUILD_DIR)/kernel.img
ELF           := $(BUILD_DIR)/kernel.elf
MAP           := $(BUILD_DIR)/kernel.map

# Флаги компиляции
CPU           := arm1176jzf-s
CFLAGS        := -mcpu=$(CPU) -marm -ffreestanding -nostdlib \
                 -mfloat-abi=soft -Wall -Wextra -O2
ASFLAGS       := -mcpu=$(CPU)
LDFLAGS       := -T linker.ld -nostdlib -Map $(MAP)

# Исходники и объекты
ASM_SRC       := boot.S
C_SRC         := kernel_main.c uart.c

ASM_OBJ       := $(BUILD_DIR)/boot.o
C_OBJ         := $(BUILD_DIR)/kernel_main.o $(BUILD_DIR)/uart.o

# Все объекты
OBJ           := $(ASM_OBJ) $(C_OBJ)


# Основная цель: собрать ядро и положить в build/
all: $(TARGET)
	@echo "=== Сборка завершена ==="
	@echo "kernel.img -> $(TARGET)"


# Создать папку build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# Ассемблер: boot.S -> boot.o
$(ASM_OBJ): $(ASM_SRC) | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@


# C: kernel_main.c -> kernel_main.o
$(BUILD_DIR)/kernel_main.o: kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# C: uart.c -> uart.o
$(BUILD_DIR)/uart.o: uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# Линковка: все .o -> kernel.elf
$(ELF): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@


# objcopy: kernel.elf -> kernel.img
$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $< $@


# Очистка
clean:
	rm -rf $(BUILD_DIR)


# Пересобрать с нуля
rebuild: clean all


# Печать информации о размере
size: $(ELF)
	$(CROSS_COMPILE)size $<


# Дизассемблировать ядро
disasm: $(ELF)
	$(CROSS_COMPILE)objdump -d $<


# Посмотреть символы
symbols: $(ELF)
	$(CROSS_COMPILE)nm $< | sort


# Посмотреть секции
sections: $(ELF)
	$(CROSS_COMPILE)objdump -h $<


.PHONY: all clean rebuild size disasm symbols sections

QEMU := qemu-system-arm
MACHINE := raspi1ap

run: $(ELF)
	$(QEMU) -M $(MACHINE) -kernel $(ELF) -display none -serial stdio -no-reboot

debug: $(ELF)
	$(QEMU) -M $(MACHINE) -kernel $(ELF) -display none -serial stdio -no-reboot -S -s

check: $(ELF)
	@$(CROSS_COMPILE)nm $(ELF) | grep -q '00008000 T _start' || (echo "FAIL"; exit 1)
	@echo "OK: точка входа на месте"

.PHONY: run debug check
