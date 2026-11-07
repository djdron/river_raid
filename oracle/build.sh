#!/bin/sh
# Builds libatari800 (headless atari800) and the rr_oracle reference runner.
# Needs: git, a C compiler (gcc or clang), make, autoconf, automake.
# Works on Linux, macOS, WSL, MSYS2.
#
# rr_oracle intercepts three emulator functions (POKEY writes, RANDOM reads and the NMI that
# starts the vertical blank). Instead of the GNU linker's --wrap, which Apple's linker lacks,
# pokey.c and cpu.c are compiled a second time with those functions renamed to __real_*;
# rr_oracle.c defines the original names, logs, and calls the __real_* versions.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
A800="$HERE/atari800"
COMMIT=98cc2dc749867c9c7317a0271ba062a6c797f886   # tested revision (2026-10-20)
CC=${CC:-cc}
EMU_CFLAGS="-g -O2 -DNETSIO"

if [ ! -d "$A800" ]; then
	git clone https://github.com/atari800/atari800.git "$A800"
	git -C "$A800" checkout -q "$COMMIT"
fi

if [ ! -f "$A800/src/libatari800.a" ]; then
	(cd "$A800" && ./autogen.sh && ./configure --target=libatari800)
	# upstream CFLAGS contain -Werror, which trips on a harmless ULONG redefinition with recent compilers
	make -C "$A800" -j4 CFLAGS="$EMU_CFLAGS"
fi

# library with the intercepted functions renamed
OBJ="$HERE/bin"
mkdir -p "$OBJ"
cp "$A800/src/libatari800.a" "$OBJ/librr_atari800.a"
(cd "$A800/src" && $CC -DHAVE_CONFIG_H -I. $EMU_CFLAGS \
	-DPOKEY_PutByte=__real_POKEY_PutByte -DPOKEY_GetByte=__real_POKEY_GetByte -c pokey.c -o "$OBJ/pokey.o")
(cd "$A800/src" && $CC -DHAVE_CONFIG_H -I. $EMU_CFLAGS -DCPU_NMI=__real_CPU_NMI -c cpu.c -o "$OBJ/cpu.o")
(cd "$OBJ" && ar rs librr_atari800.a pokey.o cpu.o)

$CC -O2 -Wall -I"$A800/src/libatari800" "$HERE/rr_oracle.c" "$OBJ/librr_atari800.a" -lm -lpthread -o "$HERE/rr_oracle"
echo "built $HERE/rr_oracle"
