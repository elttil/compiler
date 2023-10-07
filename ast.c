#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ast_t *parse_codeblock(token_t **t_orig);
ast_t *parse_primary(token_t **t_orig);
ast_t *parse_expression(token_t **t_orig);
struct BuiltinType parse_type(token_t **t_orig, int *error);
void calculate_asm_expression(ast_t *a, HashMap *m,
                              struct CompiledData **data_orig, FILE *fp);

#define ARCH_POINTER_SIZE 8

struct FunctionVariable {
  uint64_t offset;
  int is_argument;
  struct BuiltinType type;
};

HashMap *global_definitions;

const struct BuiltinType u64 = {
    .variant = builtin,
    .name = "u64",
    .byte_size = 8,
};

const struct BuiltinType u32 = {
    .variant = builtin,
    .name = "u32",
    .byte_size = 4,
};

const struct BuiltinType t_void = {
    .variant = builtin,
    .name = "u0",
    .byte_size = 0,
};

struct BuiltinType types[] = {
    u64,
    u32,
    t_void,
};

void gen_rand_string(char *s, int l) {
  int i = 0;
  for (; i < l - 1; i++)
    s[i] = (rand() % 15) + 'A';
  s[i] = '\0';
}

const char *type_to_string(struct BuiltinType t) {
  assert(t.variant == builtin);
  return t.name;
}

int builtin_functions(const char *function, ast_t *arguments) {
  if (0 == strcmp(function, "asm")) {
    printf("%s", arguments->value.string);
    return 1;
  }
  return 0;
}

void compile_binary_expression(ast_t *a, HashMap *m,
                               struct CompiledData **data_orig, FILE *fp) {
  calculate_asm_expression(a->right, m, data_orig, fp);
  fprintf(fp, "push rax\n");
  calculate_asm_expression(a->left, m, data_orig, fp);
  fprintf(fp, "pop rcx\n");
  switch (a->operator) {
  case '+':
    fprintf(fp, "add rax, rcx\n");
    break;
  case '-':
    fprintf(fp, "sub rax, rcx\n");
    break;
  case '*':
    fprintf(fp, "mul rcx\n");
    break;
  default:
    assert(0);
    break;
  }
}

void compile_function_call(ast_t *a, HashMap *m,
                           struct CompiledData **data_orig, FILE *fp,
                           int allow_builtin) {
  assert(a->value_type == string);
  if (allow_builtin) {
    int rc = builtin_functions(a->value.string, a->children);
    if (rc)
      return;
  }
  int stack_to_recover = 0;
  ast_t *arguments[10];
  int i = 0;
  for (ast_t *c = a->children; c; c = c->next, i++) {
    arguments[i] = c;
  }
  i--;
  for (; i >= 0; i--) {
    calculate_asm_expression(arguments[i], m, data_orig, fp);
    stack_to_recover += 8;
    fprintf(fp, "push rax\n");
  }
  fprintf(fp, "call %s\n", a->value.string);
  fprintf(fp, "add rsp, %d\n", stack_to_recover);
}

void compile_struct(ast_t *a, FILE *fp) {
  fprintf(fp, "section .data\n");
  assert(string == a->value_type);
  fprintf(fp, "%s:\n", a->value.string);
  for (ast_t *c = a->children; c; c = c->next) {
    struct BuiltinType type = c->statement_variable_type;
    fprintf(fp, "times %d db 0\n", type.byte_size);
  }
  fprintf(fp, "section .text\n");
  return;
}

uint64_t struct_find_member(ast_t *ast_struct, const char *member) {
  uint64_t r = 0;
  for (ast_t *c = ast_struct->children; c; c = c->next) {
    if (0 == strcmp(c->value.string, member)) {
      return r;
    }
    r += c->statement_variable_type.byte_size;
  }
  assert(0);
  return 0;
}

void compile_variable(ast_t *a, HashMap *m, FILE *fp) {
  struct FunctionVariable *ptr = hashmap_get_entry(m, (char *)a->value.string);
  // Check if we are pointing into a struct
  if (!ptr) {
    char *dot_ps = strchr(a->value.string, '.');
    if (!dot_ps) {
      assert(0 && "Unknown variable");
    }
    *dot_ps = '\0';
    ptr = hashmap_get_entry(m, (char *)a->value.string);
    assert(!ptr->is_argument && "FIXME");
    assert(ptr && "Unknown variable");
    char *member = dot_ps + 1;
    uint64_t stack_location = ptr->offset;
    uint64_t member_offset = struct_find_member(ptr->type.ast_struct, member);
    if (a->type == variable_reference) {
      member_offset = struct_find_member(ptr->type.ast_struct, member);

      fprintf(fp, "mov rax, rbp\n");
      fprintf(fp, "sub rax, 0x%lx\n", stack_location + member_offset);
      return;
    }
    fprintf(fp, "mov rax, [rbp-0x%lx]\n", stack_location + member_offset);
    return;
  }
  uint64_t stack_location = ptr->offset;
  if (ptr->is_argument) {
    if (a->type == variable_reference) {
      fprintf(fp, "mov rax, rbp\n");
      fprintf(fp, "add rax, 0x%lx\n", stack_location + 0x8);
      return;
    }
    fprintf(fp, "mov rax, [rbp+0x%lx]\n", stack_location + 0x8);
    return;
  }
  if (a->type == variable_reference) {
    fprintf(fp, "mov rax, rbp\n");
    fprintf(fp, "sub rax, 0x%lx\n", stack_location);
    return;
  }
  fprintf(fp, "mov rax, [rbp-0x%lx]\n", stack_location);
}

void compile_literal(ast_t *a, HashMap *m, struct CompiledData **data_orig,
                     FILE *fp) {
  struct CompiledData *data = *data_orig;
  if (a->value_type == num) {
    fprintf(fp, "mov rax, %ld\n", a->value.number);
  } else if (a->value_type == string) {
    if (!data) {
      data = malloc(sizeof(struct CompiledData));
      data->prev = NULL;
    } else {
      struct CompiledData *prev = data;
      data->next = malloc(sizeof(struct CompiledData));
      data = data->next;
      data->prev = prev;
    }
    data->name = malloc(10);
    gen_rand_string(data->name, 10);
    fprintf(fp, "mov rax, %s\n", data->name);
    data->buffer_size = strlen(a->value.string);
    data->buffer = malloc(data->buffer_size + 1);
    data->next = NULL;
    strcpy(data->buffer, a->value.string);
  } else {
    assert(0 && "unimplemented");
  }
  *data_orig = data;
}

void calculate_asm_expression(ast_t *a, HashMap *m,
                              struct CompiledData **data_orig, FILE *fp) {
  if (a->type == binaryexpression) {
    compile_binary_expression(a, m, data_orig, fp);
  } else if (a->type == literal) {
    compile_literal(a, m, data_orig, fp);
  } else if (a->type == function_call) {
    compile_function_call(a, m, data_orig, fp, 0);
  } else if (a->type == variable || a->type == variable_reference) {
    compile_variable(a, m, fp);
  } else {
    assert(0);
  }
}

void compile_function(ast_t *a, struct CompiledData **data_orig, FILE *fp) {
  assert(a->value_type == string);
  fprintf(fp, "%s:\n", a->value.string);
  fprintf(fp, "push rbp\n");
  fprintf(fp, "mov rbp, rsp\n");
  char *buffer;
  size_t length = 0;
  FILE *memstream = open_memstream(&buffer, &length);
  size_t s = 8;
  compile_ast(a->children, a, NULL, data_orig, memstream, &s);
  fflush(memstream);
  if (s > 8)
    fprintf(fp, "sub rsp, %ld\n", s);
  fwrite(buffer, length, 1, fp);
  fclose(memstream);
  fprintf(fp, "mov rsp, rbp\n");
  fprintf(fp, "pop rbp\n");
  fprintf(fp, "ret\n\n");
}

void compile_if_statement(ast_t *a, HashMap *m, struct CompiledData **data_orig,
                          FILE *fp, size_t *stack_size) {
  calculate_asm_expression(a->exp, m, data_orig, fp);
  fprintf(fp, "and rax, rax\n");
  char rand_string[10];
  gen_rand_string(rand_string, sizeof(rand_string));
  fprintf(fp, "jz _end_if_%s\n", rand_string);
  compile_ast(a->children, NULL, m, data_orig, fp, stack_size);
  fprintf(fp, "_end_if_%s:\n", rand_string);
}

void compile_for_statement(ast_t *a, HashMap *m,
                           struct CompiledData **data_orig, FILE *fp,
                           size_t *stack_size) {
  char for_statement_rand_string[10];
  gen_rand_string(for_statement_rand_string, sizeof(for_statement_rand_string));
  char rand_string[10];
  gen_rand_string(rand_string, sizeof(rand_string));
  fprintf(fp, "%s:\n", for_statement_rand_string);
  calculate_asm_expression(a->exp, m, data_orig, fp);
  fprintf(fp, "and rax, rax\n");
  fprintf(fp, "jz _end_if_%s\n", rand_string);
  compile_ast(a->children, NULL, m, data_orig, fp, stack_size);
  fprintf(fp, "jmp %s\n", for_statement_rand_string);
  fprintf(fp, "_end_if_%s:\n", rand_string);
}

void compile_return_statement(ast_t *a, HashMap *m,
                              struct CompiledData **data_orig, FILE *fp) {
  calculate_asm_expression(a->children, m, data_orig, fp);
  fprintf(fp, "mov rsp, rbp\n");
  fprintf(fp, "pop rbp\n");
  fprintf(fp, "ret\n\n");
}

void compile_variable_declaration(ast_t *a, HashMap *m,
                                  struct CompiledData **data_orig, FILE *fp,
                                  size_t *stack_size, uint64_t *stack) {
  *stack += a->statement_variable_type.byte_size;
  if (stack_size) {
    *stack_size += a->statement_variable_type.byte_size;
  }
  struct FunctionVariable *h = malloc(sizeof(struct FunctionVariable));
  *h = (struct FunctionVariable){
      .offset = *stack, .is_argument = 0, .type = a->statement_variable_type};
  hashmap_add_entry(m, (char *)a->value.string, h, NULL, 0);
  if (a->children) {
    calculate_asm_expression(a->children, m, data_orig, fp);
    fprintf(fp, "mov [rbp - 0x%lx], rax\n", *stack);
  }
}

void compile_variable_assignment(ast_t *a, HashMap *m,
                                 struct CompiledData **data_orig, FILE *fp) {
  struct FunctionVariable *h = hashmap_get_entry(m, (char *)a->value.string);

  // Check if we are pointing into a struct
  if (!h) {
    char *dot_ps = strchr(a->value.string, '.');
    if (!dot_ps) {
      assert(0 && "Unknown variable");
    }
    *dot_ps = '\0';
    h = hashmap_get_entry(m, (char *)a->value.string);
    assert(!h->is_argument && "FIXME");
    assert(a->type != variable_reference && "FIXME");
    assert(h && "Unknown variable");
    char *member = dot_ps + 1;
    uint64_t stack_location = h->offset;
    uint64_t member_offset = struct_find_member(h->type.ast_struct, member);
    assert(a->children);
    calculate_asm_expression(a->children, m, data_orig, fp);
    fprintf(fp, "mov [rbp - 0x%lx], rax\n", stack_location + member_offset);
    return;
  }

  assert(h && "Undefined variable.");
  uint64_t stack = h->offset;
  assert(a->children);
  calculate_asm_expression(a->children, m, data_orig, fp);
  if (!h->is_argument) {
    fprintf(fp, "mov [rbp - 0x%lx], rax\n", stack);
  } else {
    fprintf(fp, "mov [rbp + 0x%lx], rax\n", stack + 0x8);
  }
}

void compile_variable_reference_assignment(ast_t *a, HashMap *m,
                                           struct CompiledData **data_orig,
                                           FILE *fp) {
  struct FunctionVariable *h = hashmap_get_entry(m, (char *)a->value.string);
  assert(h && "Undefined variable.");
  assert(h->type.variant == pointer && "Attempting to dereference non pointer");
  uint64_t stack = h->offset;
  assert(a->children);
  calculate_asm_expression(a->children, m, data_orig, fp);
  if (!h->is_argument) {
    fprintf(fp, "mov rcx, [rbp - 0x%lx]\n", stack);
  } else {
    fprintf(fp, "mov rcx, [rbp + 0x%lx]\n", stack + 0x8);
  }
  fprintf(fp, "mov [rcx], rax\n");
}

void compile_ast(ast_t *a, ast_t *parent, HashMap *m,
                 struct CompiledData **data_orig, FILE *fp,
                 size_t *stack_size) {
  struct CompiledData *data = *data_orig;
  uint64_t stack = 0;
  if (!m)
    m = hashmap_create(10);
  if (parent && parent->args) {
    int i = 0x8;
    for (ast_t *a = parent->args; a; a = a->next) {
      assert(a->type == function_argument);
      struct FunctionVariable *h = malloc(sizeof(struct FunctionVariable));
      *h = (struct FunctionVariable){
          .offset = i, .is_argument = 1, a->statement_variable_type};
      hashmap_add_entry(m, (char *)a->value.string, h, NULL, 0);
      i += 0x8;
    }
  }
  for (; a; a = a->next) {
    switch (a->type) {
    case struct_definition:
      compile_struct(a, fp);
      break;
    case function:
      compile_function(a, &data, fp);
      break;
    case if_statement:
      compile_if_statement(a, m, &data, fp, stack_size);
      break;
    case for_statement:
      compile_for_statement(a, m, &data, fp, stack_size);
      break;
    case function_call:
      compile_function_call(a, m, &data, fp, 1);
      break;
    case return_statement:
      compile_return_statement(a, m, &data, fp);
      break;
    case variable_declaration:
      compile_variable_declaration(a, m, &data, fp, stack_size, &stack);
      break;
    case variable_assignment:
      compile_variable_assignment(a, m, &data, fp);
      break;
    case variable_reference_assignment:
      compile_variable_reference_assignment(a, m, &data, fp);
      break;
    case noop:
      break;
    default:
      assert(0 && "unimplemented");
    }
  }
  *data_orig = data;
}

uint64_t parse_number(const char *s) {
  uint64_t r = 0;
  for (; *s && isdigit(*s); s++) {
    r *= 10;
    r += *s - '0';
  }
  return r;
}

ast_t *parse_function_call_arguments(token_t **t_orig) {
  token_t *t = *t_orig;
  if (t->type == closeparen) {
    t = t->next;
    *t_orig = t;
    return NULL;
  }
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  for (; t->type != closeparen;) {
    *a = *parse_expression(&t);
    if (t->type == closeparen)
      break;
    assert(t->type == comma);
    t = t->next;

    a->next = malloc(sizeof(ast_t));
    a = a->next;
  }
  a->next = NULL;
  t = t->next;
  *t_orig = t;
  return r;
}

ast_t *parse_function_arguments(token_t **t_orig) {
  token_t *t = *t_orig;
  if (t->type == closeparen) {
    t = t->next;
    *t_orig = t;
    return NULL;
  }
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  for (; t->type != closeparen;) {
    int error;
    struct BuiltinType type = parse_type(&t, &error);
    assert(!error);
    a->type = function_argument;
    a->children = NULL;
    a->statement_variable_type = type;
    t = t->next;
    assert(t->type == alpha && "Expected name after type.");
    a->value_type = string;
    a->value.string = t->string_rep;
    t = t->next;

    a->next = NULL;
    if (t->type == closeparen)
      break;
    assert(t->type == comma);
    t = t->next;

    a->next = malloc(sizeof(ast_t));
    a = a->next;
  }
  t = t->next;
  *t_orig = t;
  return r;
}

ast_t *parse_primary(token_t **t_orig) {
  token_t *t = *t_orig;
  ast_t *r = malloc(sizeof(ast_t));
  r->next = NULL;
  r->children = NULL;
  int is_reference = 0;
  if (t->type == ampersand) {
    is_reference = 1;
    t = t->next;
  }
  if (t->type == number) {
    r->type = literal;
    r->value_type = number;
    r->value.number = parse_number(t->string_rep);
    t = t->next;
  } else if (t->type == alpha) {
    if (t->next->type == openparen) {
      r->type = function_call;
      r->value.string = t->string_rep;
      r->value_type = string;

      t = t->next;
      t = t->next;
      r->children = parse_function_call_arguments(&t);
    } else {
      if (is_reference) {
        r->type = variable_reference;
      } else {
        r->type = variable;
      }
      r->value_type = string;
      if (t->next->type == dot) {
        char *a = t->string_rep;
        uint32_t l = strlen(t->string_rep);
        t = t->next;
        l++; // dot
        t = t->next;
        l += strlen(t->string_rep);
        char *b = t->string_rep;
        r->value.string = malloc(l + 1);
        strcpy(r->value.string, a);
        strcat(r->value.string, ".");
        strcat(r->value.string, b);
      } else {
        r->value.string = t->string_rep;
      }
      t = t->next;
    }
  } else if (t->type == lexer_string) {
    r->type = literal;
    r->value_type = string;
    r->value.string = t->string_rep;
    t = t->next;
  } else {
    printf("t->type: %d\n", t->type);
    assert(0);
  }
  *t_orig = t;
  return r;
}

int precedence(token_t *t) {
  switch (t->type) {
  case plus:
    return 0;
    break;
  case minus:
    return 1;
    break;
  case star:
    return 2;
    break;
  default:
    printf("Got invalid characther %s at %u:%u, expected binaryoperator or "
           "semicolon\n",
           t->string_rep, t->line + 1, t->col);
    fflush(stdout);
    for (;;)
      ;
    break;
  }
}

char type_to_operator_char(token_enum t) {
  switch (t) {
  case plus:
    return '+';
  case minus:
    return '-';
  case star:
    return '*';
  default:
    assert(0);
    break;
  }
}

int is_end_of_expression(token_t *t) {
  return (t->type == semicolon || t->type == closeparen || t->type == comma);
}

// Future me is going to hate this code but current me likes it, because
// somehow it works.
ast_t *parse_expression_1(token_t **t_orig, ast_t *lhs, int min_prec) {
  token_t *t = *t_orig;
  token_t *operator= t;

  if (is_end_of_expression(operator))
    return lhs;

  for (; precedence(operator) >= min_prec;) {
    token_t *op_orig = operator;
    t = operator->next;
    ast_t *rhs = parse_primary(&t);
    operator= t;
    if (!is_end_of_expression(t) && !is_end_of_expression(operator)) {
      for (; precedence(operator) >= precedence(op_orig);) {
        int is_higher = (precedence(operator) > precedence(op_orig));
        rhs = parse_expression_1(&t, rhs, precedence(op_orig) + is_higher);
        operator= t;
        if (is_end_of_expression(operator)) {
          break;
        }
      }
    }
    ast_t *new_lhs = malloc(sizeof(ast_t));
    new_lhs->left = lhs;
    lhs = new_lhs;
    lhs->type = binaryexpression;
    lhs->right = rhs;
    lhs->operator= type_to_operator_char(op_orig->type);
    if (is_end_of_expression(t)) {
      break;
    }
    if (is_end_of_expression(operator)) {
      t = operator;
      break;
    }
  }
  *t_orig = t;
  return lhs;
}

ast_t *parse_expression(token_t **t_orig) {
  token_t *t = *t_orig;
  if (t->type == lexer_string) {
    ast_t *r = malloc(sizeof(ast_t));
    r->type = literal;
    r->value_type = string;
    r->value.string = t->string_rep;
    t = t->next;
    *t_orig = t;
    return r;
  }
  return parse_expression_1(t_orig, parse_primary(t_orig), 0);
}

struct BuiltinType parse_type(token_t **t_orig, int *error) {
  *error = 0;
  token_t *t = *t_orig;
  if (0 == strcmp(t->string_rep, "struct")) {
    t = t->next;
    assert(t->type == alpha);
    ast_t *a = hashmap_get_entry(global_definitions, t->string_rep);
    struct BuiltinType r;
    r.variant = structure;
    r.name = a->value.string;
    r.ast_struct = a;
    uint32_t size = 0;
    for (ast_t *c = a->children; c; c = c->next) {
      size += c->statement_variable_type.byte_size;
    }
    r.byte_size = size;
    *t_orig = t;
    return r;
  }
  for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
    if (0 == strcmp(t->string_rep, types[i].name)) {
      return types[i];
    }
  }
  *error = 1;
  return u64;
}

int parse_for(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;

  if (0 != strcmp(t->string_rep, "for"))
    return 0;

  a->type = for_statement;

  t = t->next;
  assert(t->type == openparen);
  t = t->next;

  a->exp = parse_expression(&t);
  assert(t->type == closeparen);

  t = t->next;
  assert(t->type == openbracket);

  t = t->next;
  a->children = parse_codeblock(&t);

  *t_orig = t;
  return 1;
}

int parse_if(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;

  if (0 != strcmp(t->string_rep, "if"))
    return 0;

  a->type = if_statement;

  t = t->next;
  assert(t->type == openparen);
  t = t->next;

  a->exp = parse_expression(&t);
  assert(t->type == closeparen);

  t = t->next;
  assert(t->type == openbracket);

  t = t->next;
  a->children = parse_codeblock(&t);

  *t_orig = t;
  return 1;
}

int parse_struct_definition(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;

  if (0 != strcmp(t->string_rep, "struct"))
    return 0;

  a->type = struct_definition;

  t = t->next;

  assert(t->type == alpha);
  a->value_type = string;
  a->value.string = t->string_rep;
  a->children = NULL;
  t = t->next;
  assert(t->type == openbracket);

  t = t->next;
  // Parse elements of struct
  a->children = malloc(sizeof(ast_t));
  ast_t *r = a->children;
  ast_t *prev = NULL;
  ast_t *temp = r;
  for (; t->type != closebracket;) {
    int error;
    struct BuiltinType type = parse_type(&t, &error);
    t = t->next;

    assert(!error && "Unknown type");
    temp->type = variable_declaration;
    temp->children = NULL;
    temp->statement_variable_type = type;
    assert(t->type == alpha && "Expected name after type.");
    temp->value_type = string;
    temp->value.string = t->string_rep;
    t = t->next;
    assert(t->type == comma);
    t = t->next;

    prev = temp;
    temp->next = malloc(sizeof(ast_t));
    temp = temp->next;
  }
  if (prev) {
    free(prev->next);
    prev->next = NULL;
  }
  t = t->next;
  hashmap_add_entry(global_definitions, (char *)a->value.string, a, NULL, 0);
  *t_orig = t;
  return 1;
}

int parse_variable_declaration(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;
  int error;
  struct BuiltinType type = parse_type(&t, &error);
  if (error)
    return 0;
  // Implies we are parsing <type> <something>
  // So it should be a variable declaration.
  a->type = variable_declaration;
  a->children = NULL;
  t = t->next;
  if (t->type == star) {
    struct BuiltinType *buf = malloc(sizeof(struct BuiltinType));
    memcpy(buf, &type, sizeof(struct BuiltinType));
    type.variant = pointer;
    type.ptr = buf;
    type.byte_size = ARCH_POINTER_SIZE;
    t = t->next;
  }
  a->statement_variable_type = type;
  assert(t->type == alpha && "Expected name after type.");
  a->value_type = string;
  a->value.string = t->string_rep;
  t = t->next;
  if (t->type != semicolon) {
    assert(t->type == equals && "Expected equals");
    t = t->next;
    a->children = parse_expression(&t);
    assert(t->type == semicolon);
    t = t->next;
  } else {
    t = t->next;
  }
  *t_orig = t;
  return 1;
}

int parse_variable_assignment(token_t **t_orig, ast_t *a) {
  int is_dereference = 0;
  token_t *t = *t_orig;
  if (t->type == star) {
    is_dereference = 1;
    t = t->next;
  }
  if (t->type != alpha) {
    return 0;
  }

  if (t->next->type == dot && t->next->next->type == alpha) {
    char *s1 = t->string_rep;
    t = t->next;
    t = t->next;
    char *s2 = t->string_rep;
    uint32_t l = strlen(s1) + 1 + strlen(s2);
    a->value.string = malloc(l + 1);
    strcpy(a->value.string, s1);
    strcat(a->value.string, ".");
    strcat(a->value.string, s2);
  } else if (t->next->type == equals) {
    a->value.string = t->string_rep; // alpha
  } else {
    return 0;
  }

  // Implies we are parsing <alpha> <equals> <something>("alpha = ?")
  // So it should be a variable assignment
  if (is_dereference)
    a->type = variable_reference_assignment;
  else
    a->type = variable_assignment;
  a->value_type = string;

  t = t->next; // equals
  t = t->next; // something
  a->children = parse_expression(&t);
  assert(t->type == semicolon);
  t = t->next;

  *t_orig = t;
  return 1;
}

int parse_function_call(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;
  if (t->next->type != openparen)
    return 0;
  a->type = function_call;
  a->value.string = t->string_rep;
  a->value_type = string;

  t = t->next;
  t = t->next;
  a->children = parse_function_call_arguments(&t);
  assert(t->type == semicolon && "Expeceted semicolonn");
  t = t->next;

  *t_orig = t;
  return 1;
}

int parse_builtin_statement(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
  if (t->type != alpha)
    return 0;
  // Check for builtin statement
  assert(t->string_rep);
  if (0 == strcmp(t->string_rep, "return")) {
    a->type = return_statement;
    a->next = NULL;
    t = t->next;
    a->children = parse_expression(&t);
    t = t->next;
  } else {
    assert(0 && "Expected builtin statement");
  }
  *t_orig = t;
  return 1;
}

ast_t *parse_codeblock(token_t **t_orig) {
  token_t *t = *t_orig;
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  for (; t->type != closebracket;) {
    // u64 x; OR u64 x = 5;
    if (parse_variable_declaration(&t, a))
      goto cont_for_loop;
    // x = 5;
    else if (parse_variable_assignment(&t, a))
      goto cont_for_loop;
    // if(condition) {}
    else if (parse_if(&t, a))
      goto cont_for_loop;
    // for(condition) {}
    else if (parse_for(&t, a))
      goto cont_for_loop;
    // foo(); OR foo(arg1, arg2, ...);
    else if (parse_function_call(&t, a))
      goto cont_for_loop;
    // asm(); OR return x; etc
    else if (parse_builtin_statement(&t, a))
      goto cont_for_loop;
    else
      assert(0);

  cont_for_loop:
    a->next = malloc(sizeof(ast_t));
    a = a->next;
    a->type = noop;
    a->children = NULL;
  }
  t = t->next;
  *t_orig = t;
  return r;
}

ast_t *lex2ast(token_t *t) {
  global_definitions = hashmap_create(20);
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  a->next = NULL;
  for (; t && t->type != end;) {
    if (!t || !t->next || !t->next->next)
      break;
    if (parse_struct_definition(&t, a)) {

    } else if (t->type == alpha && t->next->type == alpha &&
               t->next->next->type == openparen) { // Check function
      a->type = function;

      int error;
      a->statement_variable_type = parse_type(&t, &error);
      assert(!error);

      t = t->next;

      a->value_type = string;
      a->value.string = t->string_rep;
      t = t->next;
      t = t->next;
      a->args = parse_function_arguments(&t);

      assert(t->type == openbracket && "srror");
      t = t->next;
      a->children = parse_codeblock(&t);
    }
    a->next = malloc(sizeof(ast_t));
    a = a->next;
    a->type = noop;
    a->next = NULL;
  }
  return r;
}

int calculate_expression(ast_t *a) {
  if (a->type == binaryexpression) {
    int x = calculate_expression(a->left);
    int y = calculate_expression(a->right);
    switch (a->operator) {
    case '+':
      return x + y;
    case '-':
      return x - y;
    case '*':
      return x * y;
    default:
      assert(0);
      break;
    }
  } else if (a->type == literal) {
    if (a->value_type == (ast_value_type)number) {
      return a->value.number;
    } else {
      assert(0 && "unimplemented");
    }
  } else {
    assert(0);
  }
}

void test_calculation(void) {
  token_t *head = lexer("\
	u64 main() {\
		u64 foo = 1+2;\
		u64 bar = 4*2+1;\
		u64 zoo = 1+4*2;\
		u64 baz = func();\
		u64 booze = func()+1;\
		u64 bin = 1+func();\
		u64 fooze = 1+booze;\
		u64 rand = ooooaaa(1);\
	}");
  ast_t *h = lex2ast(head);
  assert(h->type == function);
  h = h->children;

  ast_t *c = h;
  assert(c->type == variable_declaration);
  assert(3 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  assert(9 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  assert(9 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  ast_t *f = c->children;
  assert(f->type == function_call);

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  f = c->children;
  assert(f);
  assert(f->type == binaryexpression);
  assert(f->left->type == function_call);
  assert(f->right->type == literal);

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  f = c->children;
  assert(f);
  assert(f->type == binaryexpression);
  assert(f->left->type == literal);
  assert(f->right->type == function_call);

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  f = c->children;
  assert(f);
  assert(f->type == binaryexpression);
  assert(f->left->type == literal);
  assert(f->right->type == variable);

  h = h->next;
  c = h;
  assert(c->type == variable_declaration);
  c = c->children;
  assert(c->type == function_call);
  f = c->children;
  assert(f);
  assert(f->type == literal);
  assert(f->value_type == (ast_value_type)number);
  assert(f->value.number == 1);
}
