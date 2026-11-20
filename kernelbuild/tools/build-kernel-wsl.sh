#!/usr/bin/env bash
set -euo pipefail

SRC="$1"
OUT="$2"
LD_SCRIPT="$3"
KERNEL="$4"

mkdir -p "$OUT" "$(dirname "$KERNEL")"

if command -v i686-elf-gcc >/dev/null 2>&1; then
  CC=i686-elf-gcc
  LD=i686-elf-ld
elif command -v x86_64-elf-gcc >/dev/null 2>&1; then
  CC=x86_64-elf-gcc
  LD=x86_64-elf-ld
else
  CC=gcc
  LD=ld
fi

"$CC" -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -std=c11 \
  -c "$SRC/boot.S" -o "$OUT/boot.o"
"$CC" -m32 -ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -std=c11 \
  -c "$SRC/kmain.c" -o "$OUT/kmain.o"

if "$LD" -m elf_i386 -nostdlib -T "$LD_SCRIPT" -o "$KERNEL" "$OUT/boot.o" "$OUT/kmain.o" 2>/dev/null; then
  exit 0
fi

"$CC" -m32 -ffreestanding -nostdlib -Wl,-T,"$LD_SCRIPT" -o "$KERNEL" "$OUT/boot.o" "$OUT/kmain.o"
