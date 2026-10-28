#!/bin/sh
# Convertit n'importe quelle image (JPEG, WebP, ...) en PNG utilisable par
# hackterm, qui ne sait lire que le PNG.
#
#   ./bg-convert.sh /usr/share/backgrounds/linuxmint/sele_ring.jpg
#   ./bg-convert.sh photo.jpg /chemin/sortie.png
#
# Sans second argument, la sortie va dans ~/.local/share/hackterm/<nom>.png
# et l'image est ramenée à 2560 px de large maximum (inutile de garder du
# 7680x4320 pour un fond d'écran).

set -e

in="$1"
[ -n "$in" ] || { echo "usage: $0 <image> [sortie.png]" >&2; exit 1; }
[ -f "$in" ] || { echo "introuvable : $in" >&2; exit 1; }

if [ -n "$2" ]; then
  out="$2"
else
  mkdir -p "$HOME/.local/share/hackterm"
  out="$HOME/.local/share/hackterm/$(basename "${in%.*}").png"
fi

# déjà un PNG et assez petit : rien à faire
if [ "$(head -c 8 "$in" | od -An -tx1 | tr -d ' \n')" = "89504e470d0a1a0a" ]; then
  if [ "$in" != "$out" ]; then cp "$in" "$out"; fi
  echo "$out"
  exit 0
fi

if command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -loglevel error -i "$in" -vf "scale='min(2560,iw)':-1" "$out"
elif command -v convert >/dev/null 2>&1; then
  convert "$in" -resize '2560x>' "$out"
elif command -v python3 >/dev/null 2>&1; then
  python3 - "$in" "$out" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
w, h = im.size
if w > 2560:
    im = im.resize((2560, int(h * 2560 / w)), Image.LANCZOS)
im.save(sys.argv[2], "PNG")
PY
else
  echo "aucun convertisseur disponible (ffmpeg, convert ou python3-PIL)" >&2
  exit 1
fi

echo "$out"
