#!/bin/sh
# Compile hackterm dans ./build/, via la toolchain de ~/xscreensaver-build.
#   ./scripts/build.sh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLCHAIN=${XSCREENSAVER_BUILD:-$HOME/xscreensaver-build}
[ -x "$TOOLCHAIN/build.sh" ] || {
  echo "toolchain absente : $TOOLCHAIN/build.sh" >&2
  echo "(elle fournit screenhack.h, config.h et les objets communs)" >&2
  exit 1; }
exec "$TOOLCHAIN/build.sh" "$ROOT/hacks/hackterm.c"
