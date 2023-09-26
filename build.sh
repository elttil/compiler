#!/bin/sh
cc main.c lexer.c ast.c -g -I. -Wall -pedantic -Werror || exit 1
./a.out
