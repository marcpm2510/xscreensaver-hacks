#!/bin/sh
# Build one screenhack.h-based XScreenSaver hack by hand.
# Run from the source tree's hacks/ directory:  sh scripts/build-hack.sh xmatrix2
# (copy this script into hacks/ first, or invoke it with that script path).
set -e

name=${1:?usage: build-hack.sh <hackname>}
[ -f "$name.c" ] || { echo "$name.c not found — run from the xscreensaver/hacks directory" >&2; exit 1; }

# PNG atlases are compiled in as C headers.
[ -d images/gen ] || make -C images

CF=$(pkg-config --cflags xft fontconfig freetype2 2>/dev/null)
DEFS="-DSTANDALONE -DHAVE_CONFIG_H $CF -I. -I.. -I../utils"
UTILS="blurb resources visual usleep yarandom utf8wc font-retry xmu doubletime textclient"

for u in $UTILS; do
  [ -f "../utils/$u.o" ] || gcc -O2 $DEFS -c "../utils/$u.c" -o "../utils/$u.o"
done
for o in screenhack fps ximage-loader; do
  [ -f "$o.o" ] || gcc -O2 $DEFS -c "$o.c" -o "$o.o"
done

gcc -O2 $DEFS -c "$name.c" -o "$name.o"

OBJS="$name.o screenhack.o fps.o ximage-loader.o"
for u in $UTILS; do OBJS="$OBJS ../utils/$u.o"; done

gcc -o "$name" $OBJS -lXt -lX11 -lXext -lXft -lfontconfig -lpng -lm -lutil
echo "built: $(pwd)/$name"
