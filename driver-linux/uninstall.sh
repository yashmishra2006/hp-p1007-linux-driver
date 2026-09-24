#!/bin/sh
# Restore the queue's previous PPD (saved by install.sh) and remove hp1007.
#   sudo ./uninstall.sh [queue-name]
set -e
cd "$(dirname "$0")"
QUEUE="${1:-HP_P1007}"
FILTERDIR=$(cups-config --serverbin 2>/dev/null || echo /usr/lib/cups)/filter
[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }

if [ -f "backup-$QUEUE.ppd" ]; then
    lpadmin -p "$QUEUE" -P "backup-$QUEUE.ppd"
    echo "queue $QUEUE restored to its previous driver"
fi
rm -f "$FILTERDIR/hp1007" "$FILTERDIR/hp1007enc"
rm -rf /usr/share/ppd/hp1007
