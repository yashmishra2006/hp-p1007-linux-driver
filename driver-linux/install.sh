#!/bin/sh
# Install the hp1007 CUPS driver and switch a print queue to it.
#   sudo ./install.sh [queue-name]        (default queue: HP_P1007)
# The queue's previous PPD is saved next to this script for uninstall.sh.
set -e
cd "$(dirname "$0")"
QUEUE="${1:-HP_P1007}"
FILTERDIR=$(cups-config --serverbin 2>/dev/null || echo /usr/lib/cups)/filter
PPDDIR=/usr/share/ppd/hp1007

[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 1; }

make -s
install -m 755 -o root -g root hp1007enc hp1007 "$FILTERDIR/"
install -d "$PPDDIR"
install -m 644 -o root -g root hp1007.ppd "$PPDDIR/"

# Reuse the queue's URI if it exists, otherwise find the printer on USB.
URI=$(lpstat -v "$QUEUE" 2>/dev/null | sed -n 's/^device for [^:]*: //p')
[ -z "$URI" ] && URI=$(/usr/lib/cups/backend/usb 2>/dev/null | awk '/LaserJet%20P1007/ {print $2; exit}')
[ -z "$URI" ] && { echo "P1007 not found on USB; is it on and plugged in?" >&2; exit 1; }

if [ -f "/etc/cups/ppd/$QUEUE.ppd" ] && [ ! -f "backup-$QUEUE.ppd" ]; then
    cp "/etc/cups/ppd/$QUEUE.ppd" "backup-$QUEUE.ppd"
    echo "saved old PPD to $(pwd)/backup-$QUEUE.ppd"
fi

lpadmin -p "$QUEUE" -v "$URI" -P "$PPDDIR/hp1007.ppd" -o PageSize=A4 -E
cupsenable "$QUEUE"
cupsaccept "$QUEUE"
echo "queue $QUEUE -> $URI now uses the hp1007 driver"
