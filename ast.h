typedef struct ast_struct ast_t;
#ifndef AST_H
#define AST_H
#include <hashmap/hashmap.h>
#include <lexer.h>
#include <stdint.h>
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

struct BuiltinType {
  const char *name;
  uint8_t byte_size;
};

struct CompiledData {
  char *name;
  char *buffer;
  size_t buffer_size;
  struct CompiledData *next;
  struct CompiledData *prev;
};

struct ast_struct {
  ast_enum type;
  ast_t *children;
  ast_t *next;
  char operator;
  struct BuiltinType statement_variable_type;
  ast_value_type value_type;
  ast_value value;
  ast_t *exp;
  ast_t *args;
  ast_t *left;
  ast_t *right;
};

const char *type_to_string(struct BuiltinType t);
ast_t *lex2ast(token_t *t);
void print_ast(ast_t *a);
void compile_ast(ast_t *a, ast_t *parent, HashMap *m,
                 struct CompiledData **data_orig);

void test_calculation(void);
#endif // AST_H
