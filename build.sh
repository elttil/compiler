#!/bin/sh
cc main.c lexer.c ast.c hashmap/hashmap.c hashmap/hash.c -g -I. -Wall -pedantic -Werror || exit 1
./a.out
