#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  token_t *head = lexer("\
	u64 foo() {\
		if(0) {\
			return 1;\
		}\
		return 2;\
	}\
	u64 main() {\
		return foo();\
	}\
");

  ast_t *h = lex2ast(head);
  printf("section .text\n");
  printf("_start:\n");
  printf("call main\n");
  printf("mov ebx, eax\n");
  printf("mov eax, 1\n");
  printf("int 80h\n");
  printf("\n");
  compile_ast(h);
  //  print_ast(h);
  return 0;
}
