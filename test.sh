#!/bin/sh
cc -DTESTING test.c lexer.c ast.c -I. -Wall -pedantic -Werror || exit 1
./a.out
