#!/bin/bash
# full_sd_setup.sh — форматирование SD-карты и копирование всех файлов
# Запуск: sudo bash full_sd_setup.sh

set -euo pipefail

DEV="/dev/sdc"
DEV1="${DEV}1"
MNT="/mnt/sd_check"
SRC_DIR="/home/enclave/src/Enclave_OS_arm"

echo "============================================"
echo "  Форматирование и полная настройка SD-карты"
echo "  Устрйоство: $DEV"
echo "============================================"

# 1. Размонтирование (если примонтировано)
echo ""
echo "=== Размонтирование ==="
if mountpoint -q "$MNT" 2>/dev/null; then
    echo "  Уже примонтирован. Размонтирую..."
    umount "$MNT" 2>/dev/null || true
    echo "  ✓ Размонтирован"
else
    echo "  Не примонтирован. Пропускаю."
fi

# 2. Форматирование
echo ""
echo "=== ФОРМАТИРОВАНИЕ $DEV1 ==="
echo "!!! ВНИМАНИЕ: все данные на разделе будут уничтожены !!!"
mkfs.vfat -F 32 -n boot "$DEV1"
echo "  ✓ Отформатирован в FAT32 (LABEL=boot)"

# 3. Монтирование
echo ""
echo "=== МОНТИРОВАНИЕ ==="
mkdir -p "$MNT"
mount "$DEV1" "$MNT"
echo "  ✓ Примонтирован в $MNT"

# 4. Копирование файлов
echo ""
echo "=== КОПИРОВАНИЕ ФАЙЛОВ ==="
cd "$SRC_DIR"

# firmware
if [ -d firmware ]; then
    for f in firmware/bootcode.bin firmware/start.elf firmware/fixup.dat; do
        if [ -f "$f" ]; then
            cp -v "$f" "$MNT/"
        else
            echo "  ✗ НЕ НАЙДЕН: $f"
        fi
    done
else
    echo "  ✗ Папка firmware/ не найдена!"
fi

# kernel.img
if [ -f build/kernel.img ]; then
    cp -v build/kernel.img "$MNT/"
else
    echo "  ✗ build/kernel.img НЕ НАЙДЕН!"
fi

# config.txt
if [ -f config.txt ]; then
    cp -v config.txt "$MNT/"
else
    echo "  ✗ config.txt НЕ НАЙДЕН!"
fi

# 5. Проверка содержимого
echo ""
echo "=== СОДЕРЖИМОЕ SD-КАРТЫ ==="
ls -lh "$MNT/"

# 6. Sync и отмонтирование
echo ""
echo "=== ЗАВЕРШЕНИЕ ==="
echo "  sync ..."
sync
echo "  Отмонтирую $MNT..."
umount "$MNT" 2>/dev/null && echo "  ✓ Отмонтирован" || echo "  Ошибка отмонтирования"

echo ""
echo "============================================"
echo "  ГОТОВО! Карта готова для вставки в малину."
echo "============================================"
