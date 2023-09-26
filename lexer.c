#include <assert.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void token_printtype(token_t *t) {
  switch (t->type) {
  case number:
    printf("number\n");
    break;
  case alpha:
    printf("alpha\n");
    break;
  case openparen:
    printf("openparen\n");
    break;
  case closeparen:
    printf("closeparen\n");
    break;
  case openbracket:
    printf("openbracket\n");
    break;
  case closebracket:
    printf("closebracket\n");
    break;
  case semicolon:
    printf("semicolon\n");
    break;
  case equals:
    printf("equals\n");
    break;
  case plus:
    printf("plus\n");
    break;
  case minus:
    printf("minus\n");
    break;
  case multiply:
    printf("multiply\n");
    break;
  case end:
    printf("LEXER END\n");
    break;
  }
}

const char *skip_whitespace(const char *s) {
  for (; isspace(*s) && *s != '\0'; s++)
    ;
  return s;
}

const char *parse_token(const char *s, token_t *t) {
  s = skip_whitespace(s);
  if (isalpha(*s)) {
    t->type = alpha;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    for (i = 0; *s; s++, i++) {
      if (!isalpha(*s) && !isdigit(*s))
        break;
      t->string_rep[i] = *s;
    }
    t->string_rep[i] = '\0';
    return s;
  }
  if ('(' == *s) {
    s++;
    t->type = openparen;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if (')' == *s) {
    s++;
    t->type = closeparen;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('{' == *s) {
    s++;
    t->type = openbracket;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('}' == *s) {
    s++;
    t->type = closebracket;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('=' == *s) {
    s++;
    t->type = equals;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if (';' == *s) {
    s++;
    t->type = semicolon;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 2);
    strcpy(t->string_rep, ";");
    return s;
  }
  if ('+' == *s) {
    s++;
    t->type = plus;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('*' == *s) {
    s++;
    t->type = multiply;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('\0' == *s) {
    t->type = end;
    t->next = NULL;
    t->string_rep = NULL;
    return NULL;
  }
  if (isdigit(*s)) {
    t->type = number;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    for (i = 0; *s; s++, i++) {
      if (!isdigit(*s))
        break;
      t->string_rep[i] = *s;
    }
    t->string_rep[i] = '\0';
    return s;
  }
  assert(0 && "Unknown token");
}

token_t *lexer(const char *s) {
  token_t *r = malloc(sizeof(token_t));
  token_t *t = r;
  for (;;) {
    if (!(s = parse_token(s, t)))
      break;
    t->next = malloc(sizeof(token_t));
    t = t->next;
  }
  return r;
}
