#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  FILE *fp = fopen("code.bd", "r");
  fseek(fp, 0, SEEK_END);
  size_t l = ftell(fp);
  char *buffer = malloc(l + 1);
  fseek(fp, 0, SEEK_SET);
  assert(fread(buffer, 1, l, fp) == l);
  buffer[l] = '\0';
  fclose(fp);
  printf("global _start\n");
  printf("section .text\n");
  token_t *head = lexer(buffer);

  ast_t *h = lex2ast(head);

  struct CompiledData *data;
  compile_ast(h, NULL, NULL, &data);

  printf("section .data\n");
  for (; data; data = data->next) {
    printf("%s: db '%s'\n", data->name, data->buffer);
  }
  //  print_ast(h);
  return 0;
}
