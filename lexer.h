typedef struct token_struct token_t;
#ifndef LEXER_H
#define LEXER_H

typedef enum {
  number,
  alpha,

  plus,
  minus,
  multiply,
  lexer_string,
  ampersand,
  star,

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
  token_t *next;
};

token_t *lexer(const char *s);
void token_printtype(token_t *t);
#endif // LEXER_H
