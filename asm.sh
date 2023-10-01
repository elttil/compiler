#!/bin/sh
[ -z $1 ] && echo "No file specified" && exit 1
./compiler $1 > code.asm
nasm -f elf code.asm || exit 1
gcc --freestanding -nostdlib -m32 -o code code.o || exit 1
./code
