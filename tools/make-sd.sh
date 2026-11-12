#!/usr/bin/env bash
# Готовит microSD к первому запуску Enclave OS на Raspberry Pi 1 Model B+.
#
# Деструктивно: затирает таблицу разделов и файловую систему карты целиком.
# Запуск:  sudo tools/make-sd.sh /dev/sdX
#
# Смысл проверь-и-спроси ниже: цена опечатки в имени устройства — не
# переделанная карта, а стёртый системный диск. Поэтому скрипт сначала
# печатает, что ему подставили, и требует ввести имя вручную.

set -euo pipefail

SBIN=/usr/sbin
HERE=$(cd "$(dirname "$0")" && pwd)
PAYLOAD=$(cd "$HERE/.." && pwd)/sd
FILES="bootcode.bin start.elf fixup.dat config.txt kernel.img"
MAX_BYTES=$((64 * 1024 * 1024 * 1024))   # карта больше 64 ГиБ — почти наверняка не карта

die() { echo; echo "ОТКАЗ: $*" >&2; exit 1; }

# readlink на пустой аргумент возвращает ненулевой код, а под set -e это
# убило бы скрипт раньше, чем успел напечатать подсказку по запуску.
DEV=$(readlink -f "${1:-}" || true)
[ -n "$DEV" ] || die "не указано устройство. Пример: sudo $0 /dev/sdc"
[ -b "$DEV" ] || die "$DEV — не блок-устройство"
[ -d "$PAYLOAD" ] || die "нет комплекта $PAYLOAD"

for f in $FILES; do
    [ -s "$PAYLOAD/$f" ] || die "в $PAYLOAD не хватает файла $f"
done

echo "Подготовитель SD для Enclave OS"
echo "================================"
echo "Кандидат: $DEV"
lsblk -nro NAME,RM,SIZE,TRAN,MODEL,MOUNTPOINT "$DEV"
echo

# 1. Съёмный носитель? Системный SSD/HH всегда RM=0.
#    -d обязателен: без него lsblk печатает и разделы, и сравнение с "1"
#    перестало бы работать сразу после того, как раздел появится.
[ "$(lsblk -ndro RM "$DEV")" = "1" ] \
    || die "$DEV не помечен как съёмный (RM=0) — это внутренний диск"

# 2. Рабочий стол обычно сам монтирует карту, а на примонтированную ФС
#    разметку писать нельзя. Отмонтировать умеет и обычный пользователь через
#    udisksctl — пробую сам, чтобы не заставлять вводить вторую команду.
for m in $(lsblk -nlp -o NAME,MOUNTPOINT "$DEV" | awk 'NF > 1 {print $1}'); do
    echo "отмонтирову $m"
    udisksctl unmount -b "$m" >/dev/null 2>&1 || true
done

# 3. И только если отмонтировать не вышло — стоп. Писать таблицу разделов по
#    живой файловой системе нельзя, а опечатка в имени устройства стоит не
#    переделанной карты, а стёртого диска.
if lsblk -nro MOUNTPOINT "$DEV" | grep -q '[^[:space:]]'; then
    die "устройство занято (смотри MOUNTPOINT выше) — отмонтируй: udisksctl unmount -b <раздел>"
fi

# 4. Размер. 64 ГиБ для microSD — уже потолок рынка, всё, что больше, похоже
#    на флешку-«монстр» или на диск, который мы не заметили.
BYTES=$(lsblk -ndbo SIZE "$DEV")
[ "$BYTES" -le "$MAX_BYTES" ] \
    || die "размер $((BYTES / 1024 / 1024 / 1024)) ГиБ подозрителен для microSD"

# 5. Ядро берём из build/, если оно там свежее снятого снимка: иначе после
#    ближайшей пересборки на карту уехал бы старый kernel.img молча.
if [ -s "$HERE/../build/kernel.img" ] &&
   ! cmp -s "$HERE/../build/kernel.img" "$PAYLOAD/kernel.img"; then
    cp "$HERE/../build/kernel.img" "$PAYLOAD/kernel.img"
    echo "Замечено: build/kernel.img отличается от sd/kernel.img — беру свежий."
fi


# 6. Подтверждение руками: имя печатает пользователь, а не «y» вслепую.
NAME=${DEV##*/}
printf 'Всё на %s будет УДАЛЕНО. Введите имя устройства (%s) для продолжения: ' "$DEV" "$NAME"
read -r ANSWER
[ "$ANSWER" = "$NAME" ] || die "введено '$ANSWER' вместо '$NAME' — выхожу, ничего не тронув"

echo
echo "[1/5] размечаю: MBR + один раздел W95 FAT32 (LBA) со смещения 1 МиБ"
# Смещение 2048 секторов = 1 МиБ: раз раздел начнётся на границе стирания
# флеш-памяти, карта проживёт дольше, а загрузчик Pi увидит первый раздел
# в MBR (сейчас у карты таблицы разделов нет вовсе).
"$SBIN/sfdisk" -W always "$DEV" <<'PARTSCRIPT'
label: dos
start=2048, type=c, bootable
PARTSCRIPT

echo "[2/5] жду узел раздела"
PART=""
for _ in $(seq 1 20); do
    PART=$(lsblk -nro NAME "$DEV" | sed -n '2p')
    [ -n "$PART" ] && break
    [ -x "$SBIN/partx" ] && "$SBIN"/partx -u "$NAME" >/dev/null 2>&1 || true
    sleep 0.5
done
if [ -z "$PART" ]; then
    echo "ядро не показало раздел. Переткни картридер и запусти скрипт снова —" >&2
    echo "разметка уже записана, повторный запуск просто переформатирует раздел." >&2
    exit 1
fi
PART=/dev/$PART
echo "раздел: $PART"

echo "[3/5] mkfs.vfat (32 кластера по 512 Б, метка ENCLAVE)"
"$SBIN/mkfs.vfat" -F 32 -s 32 -n ENCLAVE "$PART" >/dev/null
sync

echo "[4/5] монтирую и кладу комплект"
# udisksctl монтирует «красиво» (путь в /media/...) и из обычной сессии, но под
# root polkit часто отказывает: root-у вызов не на что опереться. Пробуем его,
# не вышло — монтируем напрямую в /mnt. Владелец файлов на FAT при этом ничего
# не значит: Pi читает файловую систему сама, без прав доступа Linux.
MOUNT=$(LC_ALL=C udisksctl mount -b "$PART" 2>/dev/null |
        sed -n 's|^.* mounted at \(.*\)\.$|\1|p' | head -1)
UMOUNT_CMD="udisksctl unmount -b $PART"
if [ -z "$MOUNT" ] && [ "$(id -u)" = "0" ]; then
    MOUNT=/mnt/enclave-sd
    mkdir -p "$MOUNT"
    mount -t vfat "$PART" "$MOUNT"
    UMOUNT_CMD="umount $MOUNT"
fi
[ -n "$MOUNT" ] && [ -d "$MOUNT" ] || die "не смог смонтировать $PART"
echo "точка монтирования: $MOUNT"

for f in $FILES; do
    cp "$PAYLOAD/$f" "$MOUNT/$f"
done
sync

echo "[5/5] сверяю контрольные суммы того, что легло на карту"
RC=0
for f in $FILES; do
    A=$(sha256sum "$PAYLOAD/$f" | cut -d' ' -f1)
    B=$(cd "$MOUNT" && sha256sum "$f" | cut -d' ' -f1)
    if [ "$A" = "$B" ]; then
        printf '  OK   %-14s %s\n' "$f" "${A:0:16}"
    else
        printf '  РАЗЛИЧАЕТСЯ %-8s карта %s != оригинал %s\n' "$f" "${B:0:16}" "${A:0:16}"
        RC=1
    fi
done

sync
# Размонтирову тем же способом, которым монтировал, и не молча: если не вышло,
# вынимать карту нельзя (FAT доживает до следующего mount в недосинте).
$UMOUNT_CMD >/dev/null 2>&1 || echo "ВНИМАНИЕ: не отмонтировалось само — umount $MOUNT"

echo
if [ "$RC" = 0 ]; then
    echo "Карта готова: $DEV ($PART), метка ENCLAVE."
    echo "kernel.img на карте: $(sha256sum "$PAYLOAD/kernel.img" | cut -d' ' -f1)"
    echo "Можно подключать UART и снимать лог."
else
    echo "Контрольные суммы разошлись — картой не пользоваться, разбираться." >&2
    exit 1
fi
