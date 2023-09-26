#!/bin/sh
./build.sh > code.asm
nasm -f elf code.asm || exit 1
gcc --freestanding -nostdlib -m32 -o code code.o || exit 1
cat code.asm
./code
echo $?
