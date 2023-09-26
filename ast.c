#include <assert.h>
#include <ast.h>
#include <ctype.h>
#include <lexer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void calculate_asm_expression(ast_t *a) {
  if (a->type == binaryexpression) {
    calculate_asm_expression(a->right);
    printf("mov ecx, ebx\n");
    calculate_asm_expression(a->left);
    switch (a->operator) {
    case '+':
      printf("add ebx, ecx\n");
      break;
    case '-':
      printf("sub ebx, ecx\n");
      break;
    case '*':
      printf("mov eax, ecx\n");
      printf("mul ebx\n");
      printf("mov ebx, eax\n");
      break;
    default:
      assert(0);
      break;
    }
  } else if (a->type == literal) {
    if (a->value_type == (ast_value_type)number) {
      printf("mov ebx, %ld\n", a->value.number);
    } else {
      assert(0 && "unimplemented");
    }
  } else if (a->type == function_call) {
    printf("call %s\n", a->value.string);
    printf("mov ebx, eax\n");
  } else {
    assert(0);
  }
}

void compile_ast(ast_t *a) {
  int stack = 0;
  for (; a; a = a->next) {
    switch (a->type) {
    case function:
      assert(a->value_type == string);
      printf("%s:\n", a->value.string);
      printf("push ebp\n");
      printf("mov ebp, esp\n");
      printf("push ebx\n");
      printf("push ecx\n");
      compile_ast(a->children);
      break;
    case function_call:
      assert(a->value_type == string);
      printf("call %s\n", a->value.string);
      break;
    case return_statement: {
      calculate_asm_expression(a->children);
      printf("mov eax, ebx\n");
      printf("pop ebx\n");
      printf("pop ecx\n");
      printf("pop ebp\n");
      printf("ret\n\n");
      break;
    }
    case variable: {
      if (a->children) {
        calculate_asm_expression(a->children);
        stack += 0x4;
        printf("mov (rbp - %x), ebx\n", stack);
        //        printf("%s %s = ", type_to_string(a->statement_variable_type),
        //               a->value.string);
        // printf("%d", calculate_expression(a->children));
        //        print_expression(a->children);
        //        printf(";\n");
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
    case variable: {
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
    assert(t->next->type == openparen);
    r->type = function_call;
    r->value.string = t->string_rep;
    r->value_type = string;

    t = t->next;
    assert(t->next->type == closeparen); // TODO parse params
    t = t->next;
    //    assert(t->next->type == semicolon && "Expeceted semicolonn");
    t = t->next;
  } else
    assert(0);
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

  if (operator->type == semicolon)
    return lhs;

  for (; precedence(operator->type) >= min_prec;) {
    token_t *op_orig = operator;
    t = operator->next;
    ast_t *rhs = parse_primary(&t);
    operator= t;
    if (operator->type != semicolon && t->type != semicolon) {
      for (; precedence(operator->type) >= precedence(op_orig->type);) {
        int is_higher =
            (precedence(operator->type) > precedence(op_orig->type));
        rhs =
            parse_expression_1(&t, rhs, precedence(op_orig->type) + is_higher);
        operator= t;
        if (operator->type == semicolon) {
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
    if (t->type == semicolon) {
      break;
    }
    if (operator->type == semicolon) {
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
        a->type = variable;
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
      } else if (t->next->type == openparen) {
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

      t = t->next->next;

      assert(t->type == closeparen && "not implemented");
      t = t->next;
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
	}");
  ast_t *h = lex2ast(head);
  assert(h->type == function);
  h = h->children;

  ast_t *c = h;
  assert(c->type == variable);
  assert(3 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable);
  assert(9 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable);
  assert(9 == calculate_expression(c->children));

  h = h->next;
  c = h;
  assert(c->type == variable);
  ast_t *f = c->children;
  assert(f->type == function_call);

  h = h->next;
  c = h;
  assert(c->type == variable);
  f = c->children;
  assert(f);
  assert(f->type == binaryexpression);
  assert(f->left->type == function_call);
  assert(f->right->type == literal);

  h = h->next;
  c = h;
  assert(c->type == variable);
  f = c->children;
  assert(f);
  assert(f->type == binaryexpression);
  assert(f->left->type == literal);
  assert(f->right->type == function_call);
}
#endif // TESTING
