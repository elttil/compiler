#include <assert.h>
#include <codegen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void calculate_asm_expression(ast_t *a, HashMap *m,
                              struct CompiledData **data_orig, FILE *fp);

static const char *matching_register_prefix(uint8_t byte_size) {
  switch (byte_size) {
  case 1:
    return ""; // 8 bits
  case 4:
    return "e"; // 32 bits
  case 8:
    return "r"; // 64 bits
  default:
    assert(0);
    break;
  }
}

void gen_rand_string(char *s, int l) {
  int i = 0;
  for (; i < l - 1; i++)
    s[i] = (rand() % 15) + 'A';
  s[i] = '\0';
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
  case '=': {
    char label[10];
    gen_rand_string(label, 10);
    fprintf(fp, "mov rdx, 0\n");
    fprintf(fp, "cmp rax, rcx\n");
    fprintf(fp, "jne %s\n", label);
    fprintf(fp, "mov rdx, 1\n");
    fprintf(fp, "%s:\n", label);
    fprintf(fp, "mov rax, rdx\n");
    break;
  }
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
    fprintf(fp, "mov %sax, [rbp-0x%lx]\n",
            matching_register_prefix(ptr->type.byte_size),
            stack_location + member_offset);
    return;
  }
  uint64_t stack_location = ptr->offset;
  if (ptr->is_argument) {
    if (a->type == variable_reference) {
      fprintf(fp, "mov rax, rbp\n");
      fprintf(fp, "add rax, 0x%lx\n", stack_location + 0x8);
      return;
    }
    fprintf(fp, "mov %sax, [rbp+0x%lx]\n",
            matching_register_prefix(ptr->type.byte_size), stack_location + 0x8);
    return;
  }
  if (a->type == variable_reference) {
    fprintf(fp, "mov rax, rbp\n");
    fprintf(fp, "sub rax, 0x%lx\n", stack_location);
    return;
  }
  fprintf(fp, "mov %sax, [rbp-0x%lx]\n",
          matching_register_prefix(ptr->type.byte_size), stack_location);
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
    fprintf(fp, "mov [rbp - 0x%lx], %sax\n", *stack,
            matching_register_prefix(h->type.byte_size));
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
    fprintf(fp, "mov [rbp - 0x%lx], %sax\n", stack_location + member_offset,
            matching_register_prefix(h->type.byte_size));
    return;
  }

  assert(h && "Undefined variable.");
  uint64_t stack = h->offset;
  assert(a->children);
  calculate_asm_expression(a->children, m, data_orig, fp);
  if (!h->is_argument) {
    fprintf(fp, "mov [rbp - 0x%lx], %sax\n", stack,
            matching_register_prefix(h->type.byte_size));
  } else {
    fprintf(fp, "mov [rbp + 0x%lx], %sax\n", stack + 0x8,
            matching_register_prefix(h->type.byte_size));
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

