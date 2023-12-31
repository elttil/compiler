#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    test_calculation();
    printf("TESTS COMPLETED");
    return 0;
  }

  FILE *fp = fopen(argv[1], "r");
  if (!fp) {
    fprintf(stderr, "File \"%s\" could not be opened.\n", argv[1]);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  size_t l = ftell(fp);
  char *buffer = malloc(l + 1);
  fseek(fp, 0, SEEK_SET);
  assert(fread(buffer, 1, l, fp) == l);
  buffer[l] = '\0';
  fclose(fp);
  printf("BITS 64\n");
  printf("global _start\n");
  printf("section .text\n");
  token_t *head = lexer(buffer);

  ast_t *h = lex2ast(head);

  struct CompiledData *data;
  size_t s;
  compile_ast(h, NULL, NULL, &data, stdout, &s);

  printf("section .data\n");
  for (; data; data = data->prev) {
    printf("%s: db ", data->name);
    for (char *b = data->buffer; *b; b++) {
      printf("0x%x, ", *b);
    }
    printf("\n");
  }
  return 0;
}
