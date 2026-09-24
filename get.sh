#!/bin/sh
# One-line installer for the HP LaserJet P1007 Linux driver:
#   curl -fsSL https://raw.githubusercontent.com/yashmishra2006/hp-p1007-linux-driver/main/get.sh | sudo sh
# Custom queue name:  ... | sudo sh -s -- MyQueue
set -e
REPO=yashmishra2006/hp-p1007-linux-driver
DEST="${HP1007_DIR:-/opt/hp-p1007-linux-driver}"

[ "$(id -u)" = 0 ] || { echo "Run as root:  curl -fsSL https://raw.githubusercontent.com/$REPO/main/get.sh | sudo sh" >&2; exit 1; }

if command -v apt-get >/dev/null 2>&1; then
    echo "==> installing dependencies (ghostscript, libjbig-dev, build tools, cups)"
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq ghostscript libjbig-dev build-essential cups curl >/dev/null
else
    echo "==> not a Debian/Ubuntu system: make sure ghostscript, libjbig (dev), a C compiler, make and cups are installed"
fi

echo "==> downloading driver to $DEST"
mkdir -p "$DEST"
curl -fsSL "https://github.com/$REPO/archive/refs/heads/main.tar.gz" | tar -xz -C "$DEST" --strip-components=1

echo "==> installing"
exec sh "$DEST/driver-linux/install.sh" "$@"
