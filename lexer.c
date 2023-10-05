#include <assert.h>
#include <ctype.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Iterator {
  const char *ptr;
  uint32_t col;
  uint32_t line;
  uint32_t last_col;
  uint32_t last_line;
  int is_eof;
};

char next(struct Iterator *i) {
  char c = *(i->ptr);
  if ('\0' == c) {
    i->is_eof = 1;
    return 0;
  }
  if ('\n' == c) {
    i->col = 0;
    i->line++;
  } else {
    i->col++;
  }
  i->ptr++;
  return c;
}

char peek(struct Iterator *i, int n) {
  char c = *(i->ptr + n);
  return c;
}

void skip_whitespace(struct Iterator *i) {
  char c;
  for (; (c = peek(i, 0)); (void)next(i)) {
    if (!isspace(c))
      break;
    i->last_col++;
  }
}

int lexer_isalpha(char c) { return isalpha(c) || c == '_'; }

void parse_token(struct Iterator *iter, token_t *t) {
#define PARSE_CHAR_TO_TOKEN(_c, _t)                                            \
  if (_c == peek(iter, 0)) {                                                   \
    (void)next(iter);                                                          \
    t->type = _t;                                                              \
    t->next = NULL;                                                            \
    t->string_rep = calloc(sizeof(char), 2);                                   \
    t->string_rep[0] = _c;                                                     \
    return;                                                                    \
  }

  skip_whitespace(iter);
  if ('/' == peek(iter, 0) && '/' == peek(iter, 1)) {
    (void)next(iter);
    (void)next(iter);
    for (;;) {
      char c = next(iter);
      if (iter->is_eof)
        return;
      if ('\n' == c)
        break;
    }
    // Recursion
    parse_token(iter, t);
    return;
  }
  if (lexer_isalpha(peek(iter, 0))) {
    t->type = alpha;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    char c;
    for (i = 0; (c = peek(iter, 0)); i++, next(iter)) {
      if (!lexer_isalpha(c) && !isdigit(c))
        break;
      t->string_rep[i] = c;
    }
    t->string_rep[i] = '\0';
    return;
  }
  if ('"' == peek(iter, 0)) {
    (void)next(iter);
    t->type = lexer_string;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    char c;
    for (i = 0; (c = peek(iter, 0)); i++) {
      if ('"' == c)
        break;
      (void)next(iter);
      if ('\\' == c) {
        char b = next(iter);
        assert(!iter->is_eof);
        switch (b) {
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
    (void)next(iter);
    return;
  }
  PARSE_CHAR_TO_TOKEN('(', openparen);
  PARSE_CHAR_TO_TOKEN(')', closeparen);
  PARSE_CHAR_TO_TOKEN('{', openbracket);
  PARSE_CHAR_TO_TOKEN('}', closebracket);
  PARSE_CHAR_TO_TOKEN('=', equals);
  PARSE_CHAR_TO_TOKEN(';', semicolon);
  PARSE_CHAR_TO_TOKEN(',', comma);
  PARSE_CHAR_TO_TOKEN('&', ampersand);
  PARSE_CHAR_TO_TOKEN('*', star);
  PARSE_CHAR_TO_TOKEN('+', plus);
  PARSE_CHAR_TO_TOKEN('-', minus);
  PARSE_CHAR_TO_TOKEN('\0', end);
  if (isdigit(peek(iter, 0))) {
    t->type = number;
    t->next = NULL;
    t->string_rep = calloc(sizeof(char), 256);
    int i;
    char c;
    for (i = 0; (c = peek(iter, 0)); i++) {
      if (!isdigit(c))
        break;
      (void)next(iter);
      t->string_rep[i] = c;
    }
    t->string_rep[i] = '\0';
    return;
  }
  printf("Rest: %s\n", iter->ptr);
  assert(0 && "Unknown token");
}

token_t *lexer(const char *s) {
  token_t *r = malloc(sizeof(token_t));
  token_t *t = r;
  struct Iterator i = {
      .ptr = s,
      .col = 0,
      .line = 0,
      .last_col = 0,
      .last_line = 0,
      .is_eof = 0,
  };
  for (;;) {
    parse_token(&i, t);
    t->col = i.last_col;
    t->line = i.last_line;
    if (i.is_eof) {
      t->type = end;
      t->next = NULL;
      break;
    }
    i.last_col = i.col;
    i.last_line = i.line;
    t->next = malloc(sizeof(token_t));
    t = t->next;
  }
  return r;
}
