#!/bin/sh
# Publie une nouvelle version de hackterm : archive + tag git + release GitHub.
#
#   scripts/release.sh 1.1.0            # publie v1.1.0
#   scripts/release.sh 1.1.0 --dry-run  # montre ce qui serait fait
#
# Le binaire est pris dans l'arbre XScreenSaver compilé (surchargeable avec
# HACKTERM_SRC). Il est strippé puis empaqueté avec le XML et bg-convert.sh.

set -e

V="$1"
[ -n "$V" ] || { echo "usage: $0 <version> [--dry-run]   ex: $0 1.1.0" >&2; exit 1; }

DRY=0
[ "$2" = "--dry-run" ] && DRY=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=${HACKTERM_SRC:-$HOME/xscreensaver/hacks}
BIN="$SRC/hackterm"
XML="$SRC/config/hackterm.xml"

[ -x "$BIN" ] || { echo "binaire absent : $BIN — compiler d'abord" >&2; exit 1; }
[ -f "$XML" ] || { echo "xml absent : $XML" >&2; exit 1; }

cd "$ROOT"

if git rev-parse -q --verify "refs/tags/v$V" >/dev/null; then
  echo "le tag v$V existe déjà" >&2
  exit 1
fi

mkdir -p dist
cp "$BIN" dist/hackterm
strip dist/hackterm 2>/dev/null || true
cp "$XML" dist/hackterm.xml
cp scripts/bg-convert.sh dist/

TAR="dist/hackterm-$V-linux-x86_64.tar.gz"
rm -f "$TAR"
tar czf "$TAR" -C dist hackterm hackterm.xml bg-convert.sh

echo "binaire : $(stat -c%s dist/hackterm) octets (strippé)"
echo "archive : $TAR ($(stat -c%s "$TAR") octets)"

if [ "$DRY" = 1 ]; then
  echo
  echo "[dry-run] git tag -a v$V -m \"Hackterm $V\" && git push origin v$V"
  echo "[dry-run] gh release create v$V $TAR --title \"Hackterm $V\" --generate-notes"
  exit 0
fi

git tag -a "v$V" -m "Hackterm $V"
git push origin "v$V"
gh release create "v$V" "$TAR" --title "Hackterm $V" --generate-notes

echo
echo "publié : https://github.com/marcpm2510/xscreensaver-hacks/releases/tag/v$V"
