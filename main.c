#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  /*
token_t *head = lexer("\
  u64 main() {\
          u64 name;\
          u64 bar = 1+2;\
          u64 ***REMOVED*** = 10+4*2+5;\
          u64 ***REMOVED*** = 5+4*2+10;\
          u64 baz = 1+func();\
          u64 booze = func()+1;\
          foo();\
  }\
");*/
  token_t *head = lexer("\
	u64 main() {\
		u64 foo = 1+2;\
		u64 bar = 4*2+1;\
		u64 zoo = 1+4*2;\
		u64 baz = func();\
		u64 booze = func()+1;\
		u64 beer = 1+func()+1;\
	}\
");

  //  for (token_t *t = head; t; t = t->next)
  //    token_printtype(t);
  ast_t *h = lex2ast(head);
  print_ast(h);
  return 0;
}
