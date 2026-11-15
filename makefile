# Enclave OS — Raspberry Pi 1 B+ (ARMv6, BCM2835, ARM1176JZF-S)
# Цель для платы: build/kernel.img (raw, грузится GPU на 0x8000).
# Цель для QEMU:  build/kernel.elf (QEMU грузит ELF по адресам сегментов;
#                 raw binary QEMU положил бы на 0x10000 — не наш адрес!).

CROSS   := arm-none-eabi-
CC      := $(CROSS)gcc
AS      := $(CROSS)as
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf
SIZE    := $(CROSS)size
NM      := $(CROSS)nm
GDB     ?= gdb-multiarch
QEMU    ?= qemu-system-arm

BUILD   := build

# ARMv6, soft-float: VFP в boot.S не включён, hard-float тут не нужен
CPUFLAGS := -mcpu=arm1176jzf-s -marm -mfloat-abi=soft
CFLAGS   := $(CPUFLAGS) -O2 -g -Wall -Wextra -std=gnu11 -ffreestanding \
            -fno-builtin -fno-stack-protector \
            -fno-unwind-tables -fno-asynchronous-unwind-tables
ASFLAGS  := -mcpu=arm1176jzf-s   # gas по умолчанию в ARM-режиме; -marm у as нет
LDFLAGS  := -nostdlib -Wl,-T,linker.ld -Wl,-Map,$(BUILD)/kernel.map

C_SRC := kernel_main.c uart.c led.c interrupt.c timer.c
OBJ   := $(BUILD)/boot.o $(patsubst %.c,$(BUILD)/%.o,$(C_SRC))
ELF   := $(BUILD)/kernel.elf
IMG   := $(BUILD)/kernel.img

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/boot.o: boot.S | $(BUILD)
	$(AS) $(ASFLAGS) -o $@ $<

# -lgcc обязателен: на ARMv6 нет аппаратного деления (__aeabi_uidivmod)
$(ELF): $(OBJ) linker.ld | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) -lgcc

$(IMG): $(ELF)
	$(OBJCOPY) -O binary $< $@

-include $(wildcard $(BUILD)/*.d)

# Проверка инвариантов: вход 0x8000, блок векторов < 0x100 (ATAG по 0x100)
check: $(ELF)
	@$(READELF) -h $(ELF) | grep 'Entry point'
	@test "$$($(READELF) -h $(ELF) | awk '/Entry point/ {print $$4}')" = "0x8000" \
	    && echo "OK: entry == 0x8000" || { echo "FAIL: entry != 0x8000"; exit 1; }
	@vs=$$($(NM) $(ELF) | awk '/ _vectors_start$$/ {print $$1}'); \
	 ve=$$($(NM) $(ELF) | awk '/ _vectors_end$$/ {print $$1}'); \
	 d=$$((0x$$ve - 0x$$vs)); echo "vectors block: $$d bytes"; \
	 test $$d -lt 256 && echo "OK: vectors < 0x100" || { echo "FAIL: vectors >= 0x100"; exit 1; }

size: $(ELF)
	$(SIZE) $(ELF)

disasm: $(ELF)
	$(OBJDUMP) -d $(ELF) > $(BUILD)/kernel.dis
	@echo "saved: $(BUILD)/kernel.dis"

symbols: $(ELF)
	$(NM) -n $(ELF)

# QEMU >= 6.2 (машины raspi0/raspi1ap). Именно ELF, не img!
qemu: $(ELF)
	$(QEMU) -M raspi1ap -kernel $(ELF) -display none -serial stdio -no-reboot

run: qemu

# 10 секунд прогона, лог в build/uart.log
qemu-log: $(ELF)
	timeout 10s $(QEMU) -M raspi1ap -kernel $(ELF) -display none \
	    -serial file:$(BUILD)/uart.log -no-reboot || true
	@cat $(BUILD)/uart.log

debug: $(ELF)
	$(QEMU) -M raspi1ap -kernel $(ELF) -display none -serial stdio -no-reboot -s -S &
	$(GDB) $(ELF) -ex 'target remote localhost:1234'

# Комплект для tools/make-sd.sh (каталог sd/ в .gitignore)
sd: $(IMG)
	mkdir -p sd
	cp firmware/bootcode.bin firmware/start.elf firmware/fixup.dat config.txt $(IMG) sd/
	@echo "sd/ готов. Дальше: sudo tools/make-sd.sh /dev/sdX"

# Обновить ядро на уже готовой карте (примонтированной)
SD_MNT ?= /media/$(USER)/ENCLAVE
install: $(IMG)
	cp $(IMG) $(SD_MNT)/kernel.img && sync
	@echo "kernel.img обновлён на $(SD_MNT)"

clean:
	rm -rf $(BUILD)

.PHONY: all check size disasm symbols qemu run qemu-log debug sd install clean
