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
  case comma:
    printf("comma\n");
    break;
  case lexer_string:
    printf("string\n");
    break;
  case ampersand:
    printf("ampersand\n");
    break;
  case star:
    printf("star\n");
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

int lexer_isalpha(char c) { return isalpha(c) || c == '_'; }

const char *parse_token(const char *s, token_t *t) {
  s = skip_whitespace(s);
  if ('/' == *s && '/' == *(s + 1)) {
    for (; *s && '\n' != *s; s++)
      ;
    if ('\n' == *s)
      s++;
    return parse_token(s, t);
  }
  if (lexer_isalpha(*s)) {
    t->type = alpha;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    for (i = 0; *s; s++, i++) {
      if (!lexer_isalpha(*s) && !isdigit(*s))
        break;
      t->string_rep[i] = *s;
    }
    t->string_rep[i] = '\0';
    return s;
  }
  if ('"' == *s) {
    s++;
    t->type = lexer_string;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    for (i = 0; *s; s++, i++) {
      if ('"' == *s)
        break;
      char c = *s;
      if ('\\' == c) {
        s++;
        assert('\0' != *s);
        switch (*s) {
        case 'n':
          c = '\n';
          break;
        case 't':
          c = '\t';
          break;
        case '\\':
          c = '\\';
          break;
        case '0':
          c = '\0';
          break;
        default:
          assert(0);
          break;
        }
      }
      t->string_rep[i] = c;
    }
    t->string_rep[i] = '\0';
    s++;
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
  if (',' == *s) {
    s++;
    t->type = comma;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 2);
    strcpy(t->string_rep, ",");
    return s;
  }
  if ('&' == *s) {
    s++;
    t->type = ampersand;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 2);
    strcpy(t->string_rep, "&");
    return s;
  }
  if ('*' == *s) {
    s++;
    t->type = star;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 2);
    strcpy(t->string_rep, "*");
    return s;
  }
  if ('+' == *s) {
    s++;
    t->type = plus;
    t->next = NULL;
    t->string_rep = NULL;
    return s;
  }
  if ('-' == *s) {
    s++;
    t->type = minus;
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
