#!/bin/bash
# check_sd.sh — диагностика SD-карты и копирование kernel.img
# Запуск: sudo bash check_sd.sh /dev/sdc

set -euo pipefail

DEV="${1:-/dev/sdc}"
DEV1="${DEV}1"
MNT="/mnt/sd_check"

echo "============================================"
echo "  Диагностика SD-карты: $DEV"
echo "============================================"

# 1. lsblk
echo ""
echo "=== 1. Блочные устройства ==="
lsblk -o NAME,MAJ:MIN,RM,SIZE,RO,TYPE,MOUNTPOINTS "$DEV" 2>/dev/null || echo "НЕ найден: $DEV"

# 2. blkid — тип ФС и метка
echo ""
echo "=== 2. Тип ФС и метка (blkid) ==="
blkid "$DEV1" 2>/dev/null || echo "  blkid не сработал на $DEV1"

# 3. fdisk — таблица разделов
echo ""
echo "=== 3. Таблица разделов (fdisk -l) ==="
fdisk -l "$DEV" 2>/dev/null || echo "  fdisk не сработал"

# 4. Размер раздела
echo ""
echo "=== 4. Размер раздела ==="
du -sh "$DEV1" 2>/dev/null || echo "  не удалось прочитать размер"

# 5. Проверяем, что это vfat
echo ""
echo "=== 5. Проверка vfat ==="
FSTYPE=$(blkid -o value -s TYPE "$DEV1" 2>/dev/null || echo "НЕИЗВЕСТНО")
echo "  Тип ФС: $FSTYPE"
if [ "$FSTYPE" = "vfat" ]; then
    echo "  ✓ vfat OK"
else
    echo "  ✗ НЕ vfat — ожидается vfat, получено: $FSTYPE"
fi

# 6. Метка раздела
echo ""
echo "=== 6. Метка раздела ==="
LABEL=$(blkid -o value -s LABEL "$DEV1" 2>/dev/null || echo "НЕИЗВЕСТНО")
echo "  Метка: $LABEL"

# 7. Монтирование
echo ""
echo "=== 7. Монтирование ==="
if mountpoint -q "$MNT" 2>/dev/null; then
    echo "  Уже примонтировано в $MNT"
else
    echo "  Не примонтировано. Пробую монтировать в $MNT ..."
    mkdir -p "$MNT"
    if mount "$DEV1" "$MNT" 2>/dev/null; then
        echo "  ✓ Примонтирован в $MNT"
    else
        echo "  ✗ Не удалось смонтировать"
        echo "  Проверь, не используется ли:"
        mount | grep "$DEV" || echo "  не используется"
    fi
fi

# 8. Содержимое (если смонтирован)
MOUNTED=false
if mountpoint -q "$MNT" 2>/dev/null; then
    MOUNTED=true
    echo ""
    echo "=== 8. Содержимое SD-карты ==="
    ls -lh "$MNT/" 2>/dev/null || echo "  каталог пуст или недоступен"
fi

# 9. Копирование kernel.img
echo ""
echo "=== 9. Копирование kernel.img ==="
KERNEL_SRC="/home/enclave/src/Enclave_OS_arm/build/kernel.img"
if [ -f "$KERNEL_SRC" ]; then
    SIZE_SRC=$(du -h "$KERNEL_SRC" | cut -f1)
    echo "  Исходник: $KERNEL_SRC ($SIZE_SRC)"
    echo "  Первый байт:"
    hexdump -C "$KERNEL_SRC" | head -1
else
    echo "  ✗ kernel.img не найден: $KERNEL_SRC"
    echo "  Сначала собери: cd /home/enclave/src/Enclave_OS_arm && make"
fi

if [ "$MOUNTED" = true ] && [ -f "$KERNEL_SRC" ]; then
    echo "  Копирую kernel.img ..."
    cp -v "$KERNEL_SRC" "$MNT/" 2>&1 && echo "  ✓ Скопирован" || echo "  ✗ Ошибка копирования"
    echo "  sync ..."
    sync
    echo "  ✓ sync выполнен"
    
    # Проверка после копирования
    echo ""
    echo "=== 10. Проверка после копирования ==="
    if [ -f "$MNT/kernel.img" ]; then
        SIZE_DST=$(du -h "$MNT/kernel.img" | cut -f1)
        echo "  kernel.img на карте: $SIZE_DST"
        echo "  Первый байт:"
        hexdump -C "$MNT/kernel.img" | head -1
    else
        echo "  ✗ kernel.img не найден на карте!"
    fi
fi

# 11. Отмонтирование
echo ""
echo "=== 11. Отмонтирование ==="
if [ "$MOUNTED" = true ] && mountpoint -q "$MNT" 2>/dev/null; then
    echo "  Отмонтирую $MNT ..."
    umount "$MNT" 2>/dev/null && echo "  ✓ Отмонтирован" || echo "  ✗ Не удалось отмонтировать"
fi

echo ""
echo "============================================"
echo "  Готово. Карта готова для вставки в малину."
echo "============================================"
