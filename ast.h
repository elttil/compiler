typedef struct ast_struct ast_t;
#ifndef AST_H
#define AST_H
#include <lexer.h>
#include <stdint.h>
#include <hashmap/hashmap.h>
typedef union {
  const char *string;
  uint64_t number;
} ast_value;

typedef enum { num, string } ast_value_type;

typedef enum {
  if_statement,
  function,
  variable,
  variable_declaration,
  variable_assignment,
  function_argument,
  literal,
  binaryexpression,
  function_call,
  return_statement,
  noop,
} ast_enum;

typedef enum {
  u64,
} builtin_types;

struct ast_struct {
  ast_enum type;
  ast_t *children;
  ast_t *next;
  char operator;
  builtin_types statement_variable_type;
  ast_value_type value_type;
  ast_value value;
  ast_t *exp;
  ast_t *args;
  ast_t *left;
  ast_t *right;
};
const char *type_to_string(builtin_types t);
ast_t *lex2ast(token_t *t);
void print_ast(ast_t *a);
void compile_ast(ast_t *a, ast_t *parent, HashMap *m);

#ifdef TESTING
void test_calculation(void);
#endif // TESTING
#endif // AST_H
