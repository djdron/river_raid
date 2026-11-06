#!/bin/sh
# Builds libatari800 (headless atari800) and the rr_oracle reference runner.
# Needs: git, gcc, make, autoconf, automake. Works on Linux, WSL, MSYS2.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
A800="$HERE/atari800"
COMMIT=98cc2dc749867c9c7317a0271ba062a6c797f886   # tested revision (2026-10-20)

if [ ! -d "$A800" ]; then
	git clone https://github.com/atari800/atari800.git "$A800"
	git -C "$A800" checkout -q "$COMMIT"
fi

if [ ! -f "$A800/src/libatari800.a" ]; then
	(cd "$A800" && ./autogen.sh && ./configure --target=libatari800)
	# upstream CFLAGS contain -Werror, which trips on a harmless ULONG redefinition with recent gcc
	make -C "$A800" -j4 CFLAGS="-g -O2 -DNETSIO"
fi

gcc -O2 -Wall -I"$A800/src/libatari800" "$HERE/rr_oracle.c" "$A800/src/libatari800.a" \
	-Wl,--wrap=POKEY_PutByte -Wl,--wrap=POKEY_GetByte -lm -lpthread -o "$HERE/rr_oracle"
echo "built $HERE/rr_oracle"
