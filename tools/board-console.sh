#!/usr/bin/env bash
# board-console.sh — консоль платы Enclave OS: UART 115200 8N1.
# Всё, что печатает ядро, дублируется на экран и в лог logs/board-<дата>.log.
#
# Использование:
#   tools/board-console.sh                 # по умолчанию /dev/ttyUSB0
#   tools/board-console.sh /dev/ttyUSB1    # другой порт
#
# Если «нет доступа»:  sudo usermod -aG dialout $USER  (и перезайти в сессию),
# либо запускать через sudo.

set -u

DEV="${1:-/dev/ttyUSB0}"
BAUD=115200
TS=$(date +%Y%m%d-%H%M%S)
LOG="logs/board-$TS.log"

die() { echo; echo "ОТКАЗ: $*" >&2; exit 1; }

[ -c "$DEV" ] || die "$DEV не найден. Проверьте адаптер: ls /dev/ttyUSB*"
[ -r "$DEV" ] && [ -w "$DEV" ] \
    || die "нет доступа к $DEV. Выполните: sudo usermod -aG dialout \$USER, перезайдите в сессию (или запустите через sudo)"
command -v stty >/dev/null || die "не найден stty (пакет coreutils)"

# 115200 8N1, сырой режим, без локального эха
stty -F "$DEV" "$BAUD" cs8 -cstopb -parenb raw -echo -echoe -echok
mkdir -p logs

echo "=== консоль Enclave OS ==="
echo "порт: $DEV @ $BAUD 8N1"
echo "лог:  $LOG"
echo "выход: Ctrl-C"
echo "подсказка: переткните питание платы (или нажмите Reset) — баннер пойдёт сначала"
echo "=========================="
echo

trap 'echo; echo "лог сохранён: $LOG"; exit 0' INT TERM

stdbuf -oL -eL cat "$DEV" | tee "$LOG"
echo
echo "порт закрылся (адаптер отключён?). лог сохранён: $LOG"
