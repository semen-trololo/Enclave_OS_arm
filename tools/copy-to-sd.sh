#!/usr/bin/env bash
# copy-to-sd.sh — безопасное (НЕдеструктивное) обновление карты для Enclave OS.
#
# Комплект ВСЕГДА собирается заново из актуальных файлов:
#   firmware/{bootcode.bin,start.elf,fixup.dat} + config.txt + build/kernel.img
# sd/ — только транзитная папка (в .gitignore). Для ПЕРВОЙ подготовки карты
# (разметка + формат) используйте tools/make-sd.sh.
#
# Использование:
#   tools/copy-to-sd.sh /dev/sdX              # скрипт сам смонтирует раздел 1
#   tools/copy-to-sd.sh /media/$USER/boot     # карта уже примонтирована

set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
PAYLOAD="$ROOT/sd"
FILES="bootcode.bin start.elf fixup.dat config.txt kernel.img"
MNT_AUTO=""   # заполняется, если монтировали сами (тогда и размонтируем сами)
PART=""

die() { echo; echo "ОТКАЗ: $*" >&2; exit 1; }

# ---------- 1. Комплект — всегда свежий ----------
[ -d "$ROOT/firmware" ]         || die "нет каталога $ROOT/firmware"
[ -s "$ROOT/build/kernel.img" ] || die "нет build/kernel.img — сначала выполните: make"

mkdir -p "$PAYLOAD"
cp -f "$ROOT/firmware/bootcode.bin" "$ROOT/firmware/start.elf" \
      "$ROOT/firmware/fixup.dat"    "$ROOT/config.txt" \
      "$ROOT/build/kernel.img"      "$PAYLOAD/"
echo "комплект освежён: прошивка + config.txt + build/kernel.img"

for f in $FILES; do
    [ -s "$PAYLOAD/$f" ] || die "в $PAYLOAD не хватает файла $f"
done

# ---------- 2. Разобрать аргумент: точка монтирования или устройство ----------
TARGET="${1:-}"
[ -n "$TARGET" ] || die "не указана цель. Примеры: $0 /dev/sdc;  $0 /media/\$USER/boot"

if [ -d "$TARGET" ]; then
    MNT=$(readlink -f "$TARGET")
elif [ -b "$TARGET" ]; then
    DEV=$(readlink -f "$TARGET")
    echo "кандидат: $DEV"
    lsblk -nro NAME,RM,SIZE,TYPE,TRAN,MODEL,MOUNTPOINT "$DEV" || true

    [ "$(lsblk -ndro RM "$DEV")" = "1" ] \
        || die "$DEV не помечен как съёмный (RM=0) — не трогаю"

    BYTES=$(lsblk -ndbo SIZE "$DEV")
    [ "$BYTES" -le $((64 * 1024 * 1024 * 1024)) ] \
        || die "размер больше 64 ГиБ — подозрительно для microSD"

    if [ "$(lsblk -ndro TYPE "$DEV")" = "part" ]; then
        PART="$DEV"
    else
        PART=$(lsblk -nro NAME "$DEV" | sed -n '2p' || true)
        [ -n "$PART" ] || die "на $DEV нет разделов. Первая подготовка: sudo tools/make-sd.sh $DEV"
        PART=/dev/$PART
    fi

    # Сначала смотрим, примонтирован ли уже раздел (авто-монт рабочего стола)
    MNT=$(lsblk -nro MOUNTPOINT "$PART" 2>/dev/null | head -1 || true)
    if [ -n "$MNT" ]; then
        echo "раздел уже примонтирован: $MNT"
    else
        FSTYPE=$(blkid -o value -s TYPE "$PART" 2>/dev/null || echo "UNKNOWN")
        [ "$FSTYPE" = "vfat" ] || [ "$FSTYPE" = "fat32" ] || [ "$FSTYPE" = "fat" ] \
            || die "$PART не vfat (получено: $FSTYPE). Первая подготовка: sudo tools/make-sd.sh $DEV"

        MNT=$(LC_ALL=C udisksctl mount -b "$PART" 2>/dev/null \
              | sed -n 's|^.* mounted at \(.*\)\.$|\1|p' | head -1 || true)
        if [ -z "$MNT" ]; then
            [ "$(id -u)" = "0" ] || die "udisksctl не смог смонтировать $PART — повторите под root"
            MNT=/mnt/enclave-sd
            mkdir -p "$MNT"
            mount -t vfat "$PART" "$MNT"
        fi
        MNT_AUTO="$MNT"
        echo "примонтировал: $MNT"
    fi
else
    die "$TARGET — не каталог (точка монтирования) и не блок-устройство"
fi

[ -d "$MNT" ] || die "точка монтирования $MNT недоступна"

# ---------- 3. Копирование ----------
echo "копирую комплект в $MNT:"
for f in $FILES; do
    cp "$PAYLOAD/$f" "$MNT/$f"
    echo "  $f"
done
sync

# ---------- 4. Сверка контрольных сумм ----------
echo "проверяю контрольные суммы:"
RC=0
for f in $FILES; do
    A=$(sha256sum "$PAYLOAD/$f" | cut -d' ' -f1)
    B=$(cd "$MNT" && sha256sum "$f" | cut -d' ' -f1)
    if [ "$A" = "$B" ]; then
        printf '  OK   %-14s %s\n' "$f" "${A:0:16}"
    else
        echo "  РАЗЛИЧАЕТСЯ $f: карта ${B:0:16} != оригинал ${A:0:16}"
        RC=1
    fi
done

# ---------- 5. Размонтирование, если монтировали сами ----------
if [ -n "$MNT_AUTO" ]; then
    echo "размонтирую $MNT_AUTO ..."
    if [ "$(id -u)" = "0" ]; then
        umount "$MNT_AUTO" || echo "ВНИМАНИЕ: не отмонтировалось — перед извлечением: umount $MNT_AUTO"
    else
        udisksctl unmount -b "$PART" >/dev/null 2>&1 \
            || echo "ВНИМАНИЕ: не отмонтировалось — перед извлечением: udisksctl unmount -b $PART"
    fi
    sync
fi

[ "$RC" -eq 0 ] || die "контрольные суммы разошлись — картой не пользоваться, разбираться"

echo
echo "Карта готова. Вставьте в Raspberry Pi, подключите UART, подайте питание."
echo "Консоль: tools/board-console.sh /dev/ttyUSB0"
