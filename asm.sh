#!/bin/sh
[ -z $1 ] && echo "No file specified" && exit 1
./compiler $1 > code.asm
nasm -f elf64 code.asm || exit 1
gcc --freestanding -nostdlib -o code code.o || exit 1
./code
