#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <hashmap/hashmap.h>
#include <lexer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ast_t *parse_codeblock(token_t **t_orig);
ast_t *parse_primary(token_t **t_orig);
builtin_types parse_type(const char *s, int *error);

struct FunctionVariable {
  uint64_t offset;
  int is_argument;
};

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

void calculate_asm_expression(ast_t *a, HashMap *m) {
  if (a->type == binaryexpression) {
    calculate_asm_expression(a->right, m);
    printf("mov ecx, eax\n");
    calculate_asm_expression(a->left, m);
    switch (a->operator) {
    case '+':
      printf("add eax, ecx\n");
      break;
    case '-':
      printf("sub eax, ecx\n");
      break;
    case '*':
      printf("mul ecx\n");
      break;
    default:
      assert(0);
      break;
    }
  } else if (a->type == literal) {
    if (a->value_type == (ast_value_type)number) {
      printf("mov eax, %ld\n", a->value.number);
    } else {
      assert(0 && "unimplemented");
    }
  } else if (a->type == function_call) {
    int stack_to_recover = 0;
    for (ast_t *c = a->children; c; c = c->next) {
      calculate_asm_expression(c, m);
      stack_to_recover += 4;
      printf("push eax\n");
    }
    printf("call %s\n", a->value.string);
    printf("add esp, %d\n", stack_to_recover);
  } else if (a->type == variable) {
    struct FunctionVariable *ptr =
        hashmap_get_entry(m, (char *)a->value.string);
    assert(ptr && "Unknown variable");
    if (!ptr->is_argument) {
      uint64_t stack_location = ptr->offset;
      printf("mov eax, [ebp-0x%lx]\n", stack_location);
    } else {
      uint64_t stack_location = ptr->offset;
      printf("mov eax, [ebp+0x%lx]\n", stack_location);
    }
  } else {
    assert(0);
  }
}

void gen_rand_string(char *s, int l) {
  int i = 0;
  for (; i < l - 1; i++)
    s[i] = (rand() % 15) + 'A';
  s[i] = '\0';
}

void compile_ast(ast_t *a, ast_t *parent) {
  uint64_t stack = 0;
  HashMap *m = hashmap_create(10);
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
      printf("%s:\n", a->value.string);
      printf("push ebp\n");
      printf("mov ebp, esp\n");
      compile_ast(a->children, a);
      break;
    case if_statement: {
      calculate_asm_expression(a->exp, m);
      printf("and eax, eax\n");
      char rand_string[10];
      gen_rand_string(rand_string, sizeof(rand_string));
      printf("jz _end_if_%s\n", rand_string);
      // TODO: Make a jump
      compile_ast(a->children, NULL);
      printf("_end_if_%s:\n", rand_string);
      break;
    }
    case function_call:
      assert(a->value_type == string);
      printf("call %s\n", a->value.string);
      break;
    case return_statement: {
      calculate_asm_expression(a->children, m);
      printf("pop ebp\n");
      printf("ret\n\n");
      break;
    }
    case variable_declaration: {
      stack += 0x4;
      struct FunctionVariable *h = malloc(sizeof(struct FunctionVariable));
      *h = (struct FunctionVariable){.offset = stack, .is_argument = 0};
      hashmap_add_entry(m, (char *)a->value.string, h, NULL, 0);
      if (a->children) {
        calculate_asm_expression(a->children, m);
        printf("mov [ebp - 0x%lx], ecx\n", stack);
      } else {
        printf("%s %s;\n", type_to_string(a->statement_variable_type),
               a->value.string);
      }
      break;
    }
    case variable_assignment: {
      uint64_t *h = hashmap_get_entry(m, (char *)a->value.string);
      assert(h && "Undefined variable.");
      uint64_t stack = *h;
      assert(a->children);
      calculate_asm_expression(a->children, m);
      printf("mov [ebp - 0x%lx], ecx\n", stack);
      break;
    }
    case noop:
      break;
    default:
      assert(0 && "unimplemented");
    }
  }
  hashmap_free(m);
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
        // printf("%d", calculate_expression(a->children));
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
    *a = *parse_primary(&t);
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
/*
      int error;
      builtin_types type = parse_type(t->string_rep, &error);
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
        */

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
    builtin_types type = parse_type(t->string_rep, &error);
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
      //    assert(t->next->type == semicolon && "Expeceted semicolonn");
    } else {
      // Variable
      r->type = variable;
      r->value_type = string;
      r->value.string = t->string_rep;
      t = t->next;
    }
  } else {
    printf("t->type: %x\n", t->type);
    assert(0);
  }
  *t_orig = t;
  return r;
}

int precedence(token_enum operator) {
  switch (operator) {
  case minus:
  case plus:
    return 0;
    break;
  case multiply:
    return 1;
    break;
  default:
    printf("Got type: ");
    token_t t = (struct token_struct){.type = operator};
    token_printtype(&t);
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
  case multiply:
    return '*';
  default:
    assert(0);
    break;
  }
}

// Future me is going to hate this code but current me likes it, because
// somehow it works.
ast_t *parse_expression_1(token_t **t_orig, ast_t *lhs, int min_prec) {
  token_t *t = *t_orig;

  token_t *operator= t;

  if (operator->type == semicolon || operator->type == closeparen)
    return lhs;

  for (; precedence(operator->type) >= min_prec;) {
    token_t *op_orig = operator;
    t = operator->next;
    ast_t *rhs = parse_primary(&t);
    operator= t;
    if (operator->type != semicolon && t->type != semicolon &&
        t->type != closeparen &&
        operator->type != closeparen) {
      for (; precedence(operator->type) >= precedence(op_orig->type);) {
        int is_higher =
            (precedence(operator->type) > precedence(op_orig->type));
        rhs =
            parse_expression_1(&t, rhs, precedence(op_orig->type) + is_higher);
        operator= t;
        if (operator->type == semicolon || operator->type == closeparen) {
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
    if (t->type == semicolon || operator->type == closeparen) {
      break;
    }
    if (operator->type == semicolon || operator->type == closeparen) {
      t = operator;
      break;
    }
  }
  *t_orig = t;
  return lhs;
}

ast_t *parse_expression(token_t **t_orig) {
  return parse_expression_1(t_orig, parse_primary(t_orig), 0);
}

builtin_types parse_type(const char *s, int *error) {
  *error = 0;
  if (0 == strcmp(s, "u64"))
    return u64;
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
    if (t->type == alpha) {
      // Check for variable
      int error;
      builtin_types type = parse_type(t->string_rep, &error);
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
          assert(t->next->type == closeparen); // TODO parse params
          a->children = NULL;
          t = t->next;
          assert(t->next->type == semicolon && "Expeceted semicolonn");
          t = t->next;
          t = t->next;
        }
      } else {
        // Check for builtin statement
        assert(t->string_rep);
        if (0 == strcmp(t->string_rep, "return")) {
          //          assert(t->next->type == (token_enum)number ||
          //                 t->next->type == (token_enum)string);
          //          assert(t->next->next->type == semicolon);
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

const char *type_to_string(builtin_types t) {
  if (t == u64)
    return "u64";
  return "UNKNOWN";
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

      // t = t->next;

      // assert(t->type == closeparen && "not implemented");
      //      t = t->next;
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

#ifdef TESTING
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
		u64 ***REMOVED*** = ooooaaa(1);\
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
  printf("call to: %s\n", c->value.string);
  f = c->children;
  assert(f);
  assert(f->type == literal);
  assert(f->value_type == (ast_value_type)number);
  assert(f->value.number == 1);
}
#endif // TESTING
