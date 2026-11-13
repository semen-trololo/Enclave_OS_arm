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
C_SRC         := kernel_main.c interrupt.c
ASM_SRC       := boot.S

# Objects
C_OBJ         := $(BUILD_DIR)/kernel_main.o $(BUILD_DIR)/interrupt.o
ASM_OBJ       := $(BUILD_DIR)/boot.o

# Target
TARGET        := kernel.elf

# Default target
all: $(BUILD_DIR)/$(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Linker script
$(BUILD_DIR)/$(TARGET): $(C_OBJ) $(ASM_OBJ) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $^

# C: kernel_main.c -> kernel_main.o
$(BUILD_DIR)/kernel_main.o: kernel_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# C: interrupt.c -> interrupt.o
$(BUILD_DIR)/interrupt.o: interrupt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly: boot.S -> boot.o
$(BUILD_DIR)/boot.o: boot.S | $(BUILD_DIR)
	$(AS) -o $@ $<

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Install
install: $(BUILD_DIR)/$(TARGET)
	cp $< /media/boot/kernel.elf

# Run in QEMU
qemu: $(BUILD_DIR)/$(TARGET)
	qemu-system-arm -M raspi1ap -kernel $< -display none -serial stdio -no-reboot

# Run with serial log
qemu-log: $(BUILD_DIR)/$(TARGET)
	qemu-system-arm -M raspi1ap -kernel $< -display none -serial file:/tmp/qemu_serial.txt -no-reboot 2>&1 &

.PHONY: all clean install qemu qemu-log