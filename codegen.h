#ifndef CODEGEN_H
#define CODEGEN_H
#include <ast.h>
#include <hashmap/hashmap.h>

void compile_ast(ast_t *a, ast_t *parent, HashMap *m,
                 struct CompiledData **data_orig, FILE *fp, size_t *stack_size);
#endif // CODEGEN_H
