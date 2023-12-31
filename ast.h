typedef struct ast_struct ast_t;
#ifndef AST_H
#define AST_H
#include <hashmap/hashmap.h>
#include <lexer.h>
#include <stdint.h>
#include <stdio.h>

typedef union {
  char *string;
  uint64_t number;
} ast_value;

typedef enum { num, string } ast_value_type;

typedef enum {
  if_statement,
  for_statement,
  struct_definition,
  function,
  variable,
  variable_reference,
  variable_declaration,
  variable_assignment,
  variable_reference_assignment,
  function_argument,
  literal,
  binaryexpression,
  function_call,
  return_statement,
  noop,
} ast_enum;

typedef enum { builtin, structure, pointer } type_variant;

struct BuiltinType {
  type_variant variant;
  const char *name;
  union {
    ast_t *ast_struct;
    struct BuiltinType *ptr;
  };
  uint8_t byte_size;
};

struct FunctionVariable {
  uint64_t offset;
  int is_argument;
  struct BuiltinType type;
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
                 struct CompiledData **data_orig, FILE *fp, size_t *stack_size);

void test_calculation(void);
#endif // AST_H
