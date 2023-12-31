typedef struct token_struct token_t;
#ifndef LEXER_H
#define LEXER_H
#include <stdint.h>

typedef enum {
  number,
  alpha,
  plus,
  minus,
  equal,
  lexer_string,
  ampersand,
  star,
  dot,
  comma,
  openparen,
  closeparen,
  openbracket,
  closebracket,
  semicolon,
  equals,
  end,
} token_enum;

struct token_struct {
  token_enum type;
  char *string_rep;
  uint32_t col;
  uint32_t line;
  token_t *next;
};

token_t *lexer(const char *s);
#endif // LEXER_H
