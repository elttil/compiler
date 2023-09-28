#!/bin/sh
cc -DTESTING test.c lexer.c ast.c hashmap/hashmap.c hashmap/hash.c -I. -Wall -pedantic -Werror || exit 1
./a.out
