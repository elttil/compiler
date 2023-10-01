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
struct BuiltinType parse_type(const char *s, int *error);

struct FunctionVariable {
  uint64_t offset;
  int is_argument;
};

const struct BuiltinType u64 = {
    .name = "u64",
    .byte_size = 8,
};

const struct BuiltinType u32 = {
    .name = "u32",
    .byte_size = 4,
};

const struct BuiltinType t_void = {
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

const char *type_to_string(struct BuiltinType t) { return t.name; }

void print_expression(ast_t *a) {
  if (a->type == binaryexpression) {
    print_expression(a->left);
    printf(" %c ", a->operator);
    print_expression(a->right);
  } else if (a->type == literal) {
    if (a->value_type == (ast_value_type)number) {
      printf("%lu", a->value.number);
    } else if (a->value_type == string) {
      printf("\"%s\"", a->value.string);
    } else {
      assert(0 && "unimplemented");
    }
  } else if (a->type == function_call) {
    assert(a->value_type == string);
    printf("%s()", a->value.string);
  } else {
    assert(0);
  }
}

int builtin_functions(const char *function, ast_t *arguments) {
  if (0 == strcmp(function, "asm")) {
    printf("%s", arguments->value.string);
    return 1;
  }
  return 0;
}

void calculate_asm_expression(ast_t *a, HashMap *m,
                              struct CompiledData **data_orig, FILE *fp) {
  struct CompiledData *data = *data_orig;
  if (a->type == binaryexpression) {
    calculate_asm_expression(a->right, m, &data, fp);
    fprintf(fp, "mov ecx, eax\n");
    calculate_asm_expression(a->left, m, &data, fp);
    switch (a->operator) {
    case '+':
      fprintf(fp, "add eax, ecx\n");
      break;
    case '-':
      fprintf(fp, "sub eax, ecx\n");
      break;
    case '*':
      fprintf(fp, "mul ecx\n");
      break;
    default:
      assert(0);
      break;
    }
  } else if (a->type == literal) {
    if (a->value_type == num) {
      fprintf(fp, "mov eax, %ld\n", a->value.number);
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
      fprintf(fp, "mov eax, %s\n", data->name);
      data->buffer_size = strlen(a->value.string);
      data->buffer = malloc(data->buffer_size + 1);
      data->next = NULL;
      strcpy(data->buffer, a->value.string);
    } else {
      assert(0 && "unimplemented");
    }
  } else if (a->type == function_call) {
    int stack_to_recover = 0;
    ast_t *arguments[10];
    int i = 0;
    for (ast_t *c = a->children; c; c = c->next, i++) {
      arguments[i] = c;
    }
    i--;
    for (; i >= 0; i--) {
      calculate_asm_expression(arguments[i], m, &data, fp);
      stack_to_recover += 4;
      fprintf(fp, "push eax\n");
    }
    fprintf(fp, "call %s\n", a->value.string);
    fprintf(fp, "add esp, %d\n", stack_to_recover);
  } else if (a->type == variable || a->type == variable_reference) {
    struct FunctionVariable *ptr =
        hashmap_get_entry(m, (char *)a->value.string);
    assert(ptr && "Unknown variable");
    if (!ptr->is_argument) {
      uint64_t stack_location = ptr->offset;
      if (a->type == variable_reference) {
        fprintf(fp, "mov eax, ebp\n");
        fprintf(fp, "sub eax, 0x%lx\n", stack_location);
      } else {
        fprintf(fp, "mov eax, [ebp-0x%lx]\n", stack_location);
      }
    } else {
      uint64_t stack_location = ptr->offset;
      if (a->type == variable_reference) {
        fprintf(fp, "mov eax, ebp\n");
        fprintf(fp, "add eax, 0x%lx\n", stack_location);
      } else {
        fprintf(fp, "mov eax, [ebp+0x%lx]\n", stack_location);
      }
    }
  } else {
    assert(0);
  }
  *data_orig = data;
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
      *h = (struct FunctionVariable){.offset = i, .is_argument = 1};
      hashmap_add_entry(m, (char *)a->value.string, h, NULL, 0);
      i += 0x4;
    }
  }
  for (; a; a = a->next) {
    switch (a->type) {
    case function:
      assert(a->value_type == string);
      fprintf(fp, "%s:\n", a->value.string);
      fprintf(fp, "push ebp\n");
      fprintf(fp, "mov ebp, esp\n");
      char *buffer;
      size_t length = 0;
      FILE *memstream = open_memstream(&buffer, &length);
      size_t s = 4;
      compile_ast(a->children, a, NULL, &data, memstream, &s);
      fflush(memstream);
      if (s > 4)
        fprintf(fp, "sub esp, %ld\n", s);
      fwrite(buffer, length, 1, fp);
      fclose(memstream);
      fprintf(fp, "mov esp, ebp\n");
      fprintf(fp, "pop ebp\n");
      fprintf(fp, "ret\n\n");
      break;
    case if_statement: {
      calculate_asm_expression(a->exp, m, &data, fp);
      fprintf(fp, "and eax, eax\n");
      char rand_string[10];
      gen_rand_string(rand_string, sizeof(rand_string));
      fprintf(fp, "jz _end_if_%s\n", rand_string);
      // TODO: Make a jump
      compile_ast(a->children, NULL, m, &data, fp, stack_size);
      fprintf(fp, "_end_if_%s:\n", rand_string);
      break;
    }
    case function_call:
      assert(a->value_type == string);
      int rc = builtin_functions(a->value.string, a->children);
      if (!rc) {
        int stack_to_recover = 0;
        ast_t *arguments[10];
        int i = 0;
        for (ast_t *c = a->children; c; c = c->next, i++) {
          arguments[i] = c;
        }
        i--;
        for (; i >= 0; i--) {
          calculate_asm_expression(arguments[i], m, &data, fp);
          stack_to_recover += 4;
          fprintf(fp, "push eax\n");
        }
        fprintf(fp, "call %s\n", a->value.string);
        fprintf(fp, "add esp, %d\n", stack_to_recover);
      }
      break;
    case return_statement: {
      calculate_asm_expression(a->children, m, &data, fp);
      fprintf(fp, "mov esp, ebp\n");
      fprintf(fp, "pop ebp\n");
      fprintf(fp, "ret\n\n");
      break;
    }
    case variable_declaration: {
      stack += 0x4;
      if (s)
        *stack_size += 0x4;
      struct FunctionVariable *h = malloc(sizeof(struct FunctionVariable));
      *h = (struct FunctionVariable){.offset = stack, .is_argument = 0};
      hashmap_add_entry(m, (char *)a->value.string, h, NULL, 0);
      if (a->children) {
        calculate_asm_expression(a->children, m, &data, fp);
        fprintf(fp, "mov [ebp - 0x%lx], eax\n", stack);
      } else {
        fprintf(fp, "%s %s;\n", type_to_string(a->statement_variable_type),
                a->value.string);
      }
      break;
    }
    case variable_assignment: {
      struct FunctionVariable *h =
          hashmap_get_entry(m, (char *)a->value.string);
      assert(h && "Undefined variable.");
      uint64_t stack = h->offset;
      assert(a->children);
      calculate_asm_expression(a->children, m, &data, fp);
      if (!h->is_argument) {
        fprintf(fp, "mov [ebp - 0x%lx], eax\n", stack);
      } else {
        fprintf(fp, "mov [ebp + 0x%lx], eax\n", stack);
      }
      break;
    }
    case variable_reference_assignment: {
      struct FunctionVariable *h =
          hashmap_get_entry(m, (char *)a->value.string);
      assert(h && "Undefined variable.");
      uint64_t stack = h->offset;
      assert(a->children);
      calculate_asm_expression(a->children, m, &data, fp);
      if (!h->is_argument) {
        fprintf(fp, "mov ecx, [ebp - 0x%lx]\n", stack);
      } else {
        fprintf(fp, "mov ecx, [ebp + 0x%lx]\n", stack);
      }
      fprintf(fp, "mov [ecx], eax\n");
      break;
    }
    case noop:
      break;
    default:
      assert(0 && "unimplemented");
    }
  }
  *data_orig = data;
}

void print_ast(ast_t *a) {
  for (; a; a = a->next) {
    switch (a->type) {
    case function:
      assert(a->value_type == string);
      printf("function (%s) -> %s{\n", a->value.string,
             type_to_string(a->statement_variable_type));
      print_ast(a->children);
      printf("}\n");
      break;
    case function_call:
      assert(a->value_type == string);
      printf("calling function: %s\n", a->value.string);
      break;
    case return_statement: {
      printf("return: ");
      print_expression(a->children);
      printf("\n");
      break;
    }
    case variable_declaration: {
      if (a->children) {
        printf("%s %s = ", type_to_string(a->statement_variable_type),
               a->value.string);
        print_expression(a->children);
        printf(";\n");
      } else {
        printf("%s %s;\n", type_to_string(a->statement_variable_type),
               a->value.string);
      }
    } break;
    case noop:
      break;
    default:
      assert(0 && "unimplemented");
    }
  }
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
    struct BuiltinType type = parse_type(t->string_rep, &error);
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
      r->value.string = t->string_rep;
      t = t->next;
    }
  } else if (t->type == lexer_string) {
    r->type = literal;
    r->value_type = string;
    r->value.string = t->string_rep;
    t = t->next;
  } else {
    printf("t->type: %x\n", t->type);
    assert(0);
  }
  *t_orig = t;
  return r;
}

int precedence(token_t *t) {
  switch (t->type) {
  case minus:
  case plus:
    return 0;
    break;
  case star:
    return 1;
    break;
  default:
    printf("Got type: ");
    token_printtype(t);
    printf("\n");
    assert(0);
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

struct BuiltinType parse_type(const char *s, int *error) {
  *error = 0;
  for (int i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
    if (0 == strcmp(s, types[i].name)) {
      return types[i];
    }
  }
  *error = 1;
  return u64;
}

int parse_if(token_t **t_orig, ast_t *a) {
  token_t *t = *t_orig;
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

ast_t *parse_codeblock(token_t **t_orig) {
  token_t *t = *t_orig;
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  for (; t->type != closebracket;) {
    int is_dereference = 0;
    if (t->type == star) {
      is_dereference = 1;
      t = t->next;
    }
    if (t->type == alpha) {
      // Check for variable
      int error;
      struct BuiltinType type = parse_type(t->string_rep, &error);
      if (!error) {
        // Implies we are parsing <type> <something>
        // So it should be a variable declaration.
        a->type = variable_declaration;
        a->children = NULL;
        a->statement_variable_type = type;
        t = t->next;
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
      } else if (t->next->type == equals) {
        // Implies we are parsing <alpha> <equals> <something>("alpha = ?")
        // So it should be a variable assignment
        if (is_dereference)
          a->type = variable_reference_assignment;
        else
          a->type = variable_assignment;
        a->value_type = string;
        a->value.string = t->string_rep; // alpha

        t = t->next; // equals
        t = t->next; // something
        a->children = parse_expression(&t);
        assert(t->type == semicolon);
        t = t->next;
      } else if (t->next->type == openparen) {
        int worked = parse_if(&t, a);
        if (!worked) {
          a->type = function_call;
          a->value.string = t->string_rep;
          a->value_type = string;

          t = t->next;
          t = t->next;
          a->children = parse_function_call_arguments(&t);
          assert(t->type == semicolon && "Expeceted semicolonn");
          t = t->next;
        }
      } else {
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
      }
    }
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
  ast_t *r = malloc(sizeof(ast_t));
  ast_t *a = r;
  a->next = NULL;
  for (; t && t->type != end;) {
    // Check function
    if (t->type == alpha && t->next->type == alpha &&
        t->next->next->type == openparen) {
      a->type = function;

      int error;
      a->statement_variable_type = parse_type(t->string_rep, &error);
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
