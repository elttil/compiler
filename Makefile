CFLAGS=-g -I. -Wall -pedantic -Werror
OBJ=main.o lexer.o ast.o hashmap/hashmap.o
all: compiler

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

compiler: $(OBJ)
	$(CC) $^ -o compiler

test: compiler
	./compiler

clean:
	rm $(OBJ) compiler ./test_compiler
