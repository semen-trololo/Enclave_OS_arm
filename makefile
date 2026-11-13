# Enclave OS Makefile

CC            := arm-none-eabi-gcc
AS            := arm-none-eabi-as
LD            := arm-none-eabi-ld

# Flags
CFLAGS        := -Wall -Wextra -nostdlib -nostartfiles -ffreestanding -march=armv6 -mfloat-abi=hard -mfpu=vfp
LDFLAGS       := -T linker.ld

# Directories
BUILD_DIR     := build
SRC_DIR       := .

# Files
C_SRC     := kernel_main.c interrupt.c uart.c led.c
ASM_SRC       := boot.S

# Objects
C_OBJ         := $(BUILD_DIR)/kernel_main.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/uart.o $(BUILD_DIR)/led.o
ASM_OBJ       := $(BUILD_DIR)/boot.o

# Target
TARGET        := kernel.elf
IMG           := kernel.img

# Default target
all: $(BUILD_DIR)/$(IMG)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Linker script
$(BUILD_DIR)/$(TARGET): $(C_OBJ) $(ASM_OBJ) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $^

# Convert ELF to raw binary for Raspberry Pi
$(BUILD_DIR)/$(IMG): $(BUILD_DIR)/$(TARGET)
	arm-none-eabi-objcopy -O binary $< $@

# C: kernel_main.c -> kernel_main.o
$(BUILD_DIR)/kernel_main.o: kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C: interrupt.c -> interrupt.o
$(BUILD_DIR)/interrupt.o: interrupt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C: uart.c -> uart.o
$(BUILD_DIR)/uart.o: uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C: led.c -> led.o
$(BUILD_DIR)/led.o: led.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly: boot.S -> boot.o
$(BUILD_DIR)/boot.o: boot.S | $(BUILD_DIR)
	$(AS) -o $@ $<

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install
install: $(BUILD_DIR)/$(IMG)
	cp $< /media/boot/kernel.img

# Run in QEMU
qemu: $(BUILD_DIR)/$(TARGET)
	qemu-system-arm -M raspi1ap -kernel $< -display none -serial stdio -no-reboot

# Run with serial log
qemu-log: $(BUILD_DIR)/$(TARGET)
	qemu-system-arm -M raspi1ap -kernel $< -display none -serial file:/tmp/qemu_serial.txt -no-reboot 2>&1 &

.PHONY: all clean install qemu qemu-log