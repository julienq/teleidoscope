#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_STRING_SIZE 256

// The lexer returns tokens [0-255] for unknown characters, and negative values
// as follows for known tokens
enum Token {
  tok_eof = -1,         // end of file
  tok_def = -2,         // def keyword
  tok_extern = -3,      // extern keyword
  tok_identifier = -4,  // identifier (see id_string)
  tok_number = -5,      // number (see num_val)
  tok_if = -6,          // if keyword
  tok_then = -7,        // then keyword
  tok_else = -8,        // else keyword
};

// AST node types
enum {
  ast_number = 0,  // val = double
  ast_var,         // val = string
  ast_binary,      // val = op, left, right
  ast_call,        // val = string (callee), left = first arg (then next)
  ast_prototype,   // val = identifiers (name, ...)
  ast_function,    // left = proto (ast_prototype), right = body
  ast_if           // val = cond, left = then, right = else
};

#pragma clang diagnostic ignored "-Wpadded"

// List of identifiers, used for prototypes, as well as the lookup list for
// functions (extern, stdlib, user-defined)
typedef struct id_list {
  char *id;
  int type;
  struct id_list *next;
} id_list;

// Identifier type bits
enum {
  id_extern = 1,
  id_stdlib = 2,
  id_math = 4,
  id_function = 8,
  id_used = 16
};

typedef struct ast_node {
  int type;
  union {
    int op;
    double number;
    char *string;
    id_list *identifiers;
    struct ast_node *node;
  } val;
  struct ast_node *left;
  struct ast_node *right;
  struct ast_node *next;
} ast_node;

id_list *lookup(id_list *, char *);
id_list *prepend_id(id_list *, char *, int);
int gettok(void);
int get_next_token(void);
ast_node *error(const char *);
ast_node *parse_number_expr(void);
ast_node *parse_paren_expr(ast_node *);
ast_node *parse_identifier_expr(ast_node *);
ast_node *parse_if_expr(ast_node *);
ast_node *parse_primary(ast_node *);
int get_tok_precedence(void);
ast_node *parse_expression(ast_node *);
ast_node *parse_binop_rhs(ast_node *, int, ast_node *);
ast_node *parse_prototype(void);
ast_node *parse_definition(ast_node *p);
ast_node *parse_extern(void);
ast_node *parse_top_level_expr(ast_node *);
ast_node *parse(void);
void output_def(ast_node *);
void output_expr(ast_node *, bool);
bool output_extern(id_list *);
void output_main(ast_node *);
void output(ast_node *);


// Utility

id_list *lookup(id_list *l, char *id) {
  for (; l; l = l->next) {
    if (strcmp(l->id, id) == 0) {
      return l;
    }
  }
  return NULL;
}

id_list *prepend_id(id_list *l, char *id, int type) {
  id_list *ll = (id_list *)malloc(sizeof(id_list));
  ll->id = id;
  ll->type = type;
  ll->next = l;
  return ll;
}

// Lexer

char tok_string[ID_STRING_SIZE];
double num_val;

int gettok(void) {
  static int last_char = ' ';
  size_t i = 0;
  while (isspace(last_char)) {
    last_char = getchar();
  }
  if (isalpha(last_char)) {
    // identifier: [a-zA-Z][a-zA-Z0-9]*
    tok_string[i++] = (char) last_char;
    while (isalnum(last_char = getchar())) {
      tok_string[i++] = (char) last_char;
    }
    tok_string[i] = '\0';
    if (strcmp(tok_string, "def") == 0) {
      return tok_def;
    }
    if (strcmp(tok_string, "extern") == 0) {
      return tok_extern;
    }
    if (strcmp(tok_string, "if") == 0) {
      return tok_if;
    }
    if (strcmp(tok_string, "then") == 0) {
      return tok_then;
    }
    if (strcmp(tok_string, "else") == 0) {
      return tok_else;
    }
    return tok_identifier;
  }
  if (isdigit(last_char) || last_char == '.') {
    // number [0-9.]+
    do {
      tok_string[i++] = (char) last_char;
      last_char = getchar();
    } while (isdigit(last_char) || last_char == '.');
    tok_string[i] = '\0';
    num_val = strtod(tok_string, NULL);
    return tok_number;
  }
  if (last_char == '#') {
    // comment until the end of the line
    do {
      last_char = getchar();
    } while (last_char != EOF && last_char != '\n');
    if (last_char != EOF) {
      return gettok();
    }
  }
  if (last_char == EOF) {
    return tok_eof;
  }
  int this_char = last_char;
  last_char = getchar();
  return this_char;
}


// Parser

static int cur_tok;

int get_next_token(void) {
  return cur_tok = gettok();
}

ast_node *error(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  return NULL;
}

// numberexpr ::= number
ast_node *parse_number_expr(void) {
  ast_node *result = (ast_node *)malloc(sizeof(ast_node));
  if (result) {
    result->type = ast_number;
    result->val.number = num_val;
  }
  get_next_token();
  return result;
}

// parenexpr ::= '(' expression ')'
ast_node *parse_paren_expr(ast_node *p) {
  get_next_token();
  ast_node *v = parse_expression(p);
  if (v) {
    if (cur_tok != ')') {
      return error("expected ')'");
    } else {
      get_next_token();
    }
  }
  return v;
}

// identifierexpr ::= identifier
//                ::= identifier '(' expression* ')'
ast_node *parse_identifier_expr(ast_node *p) {
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->val.string = strdup(tok_string);
  // Mark the identifier as being used so that it will be exported
  id_list *l = lookup(p->val.identifiers, n->val.string);
  if (l) {
    l->type |= id_used;
  }
  get_next_token();
  if (cur_tok != '(') {
    n->type = ast_var;
    return n;
  }
  get_next_token();
  n->type = ast_call;
  ast_node *argp = NULL;
  if (cur_tok != ')') {
    while (1) {
      ast_node *arg = parse_expression(p);
      if (!arg) {
        return NULL;
      }
      if (argp) {
        argp->next = arg;
        argp = argp->next;
      } else {
        n->left = arg;
        argp = arg;
      }
      if (cur_tok == ')') {
        break;
      }
      if (cur_tok != ',') {
        return error("Expected ')' or ',' in argument list");
      }
      get_next_token();
    }
  }
  get_next_token();
  return n;
}

ast_node *parse_if_expr(ast_node *p) {
  get_next_token();
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->type = ast_if;
  n->val.node = parse_expression(p);
  if (!n->val.node) {
    return NULL;
  }
  if (cur_tok != tok_then) {
    return error("expected then");
  }
  get_next_token();
  n->left = parse_expression(p);
  if (!n->left) {
    return NULL;
  }
  if (cur_tok != tok_else) {
    return error("expected else");
  }
  get_next_token();
  n->right = parse_expression(p);
  if (!n->right) {
    return NULL;
  }
  return n;
}

// primary ::= identifierexpr
//         ::= ifexpr
//         ::= numberexpr
//         ::= parenexpr
ast_node *parse_primary(ast_node *p) {
  switch (cur_tok) {
    case tok_identifier: return parse_identifier_expr(p);
    case tok_number: return parse_number_expr();
    case tok_if: return parse_if_expr(p);
    case '(': return parse_paren_expr(p);
    default:
      get_next_token();
      return error("unknown token when expecting an expression");
  }
}

// TODO replace with lookup for user-defined operators
int get_tok_precedence(void) {
  switch (cur_tok) {
    case '<': return 10;
    case '+': return 20;
    case '-': return 20;
    case '*': return 40;
    default:  return -1;
  }
}

// expression ::= primary binoprhs
ast_node *parse_expression(ast_node *p) {
  ast_node *lhs = parse_primary(p);
  return lhs ? parse_binop_rhs(p, 0, lhs) : NULL;
}

// binoprhs ::= ('+' primary)*
ast_node *parse_binop_rhs(ast_node *p, int expr_prec, ast_node *lhs) {
  while (1) {
    int tok_prec = get_tok_precedence();
    if (tok_prec < expr_prec) {
      return lhs;
    }
    int bin_op = cur_tok;
    get_next_token();
    ast_node *rhs = parse_primary(p);
    if (!rhs) {
      return NULL;
    }
    int next_prec = get_tok_precedence();
    if (tok_prec < next_prec) {
      rhs = parse_binop_rhs(p, tok_prec + 1, rhs);
      if (!rhs) {
        return NULL;
      }
    }
    ast_node *n = (ast_node *)malloc(sizeof(ast_node));
    if (n) {
      n->type = ast_binary;
      n->val.op = bin_op;
      n->left = lhs;
      n->right = rhs;
    }
    lhs = n;
  }
}

// prototype ::= id '(' id* ')'
ast_node *parse_prototype(void) {
  if (cur_tok != tok_identifier) {
    return error("Expected function name in prototype");
  }
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->type = ast_prototype;
  n->val.identifiers = (id_list *)malloc(sizeof(id_list));
  n->val.identifiers->id = strdup(tok_string);
  n->val.identifiers->next = NULL;
  get_next_token();
  if (cur_tok != '(') {
    return error("Expected '(' in prototype");
  }
  id_list *lp = n->val.identifiers;
  while (get_next_token() == tok_identifier) {
    lp->next = (id_list *)malloc(sizeof(id_list));
    lp->next->id = strdup(tok_string);
    lp = lp->next;
  }
  if (cur_tok != ')') {
    return error("Expected ')' in prototype");
  }
  get_next_token();
  return n;
}

// definition ::= 'def' prototype expression
ast_node *parse_definition(ast_node *p) {
  get_next_token();
  ast_node *proto = parse_prototype();
  if (!proto) {
    return NULL;
  }
  ast_node *e = parse_expression(p);
  if (!e) {
    return NULL;
  }
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->type = ast_function;
  n->left = proto;
  n->right = e;
  return n;
}

// external ::= 'extern' prototype
ast_node *parse_extern() {
  get_next_token();
  return parse_prototype();
}

void output_expr(ast_node *expr, bool returns) {
  if (!expr) {
    return;
  }
  if (returns && expr->type != ast_if) {
    printf("return ");
  }
  switch (expr->type) {
    case ast_number:
      if (expr->val.number - (double)(int)expr->val.number == 0.0) {
        printf("%g.", expr->val.number);
      } else {
        printf("%g", expr->val.number);
      }
      break;
    case ast_var:
      printf("%s", expr->val.string);
      break;
    case ast_binary:
      printf("(");
      output_expr(expr->left, false);
      printf(" %c ", (char) expr->val.op);
      output_expr(expr->right, false);
      printf(")");
      break;
    case ast_call:
      printf("+%s(", expr->val.string);
      for (ast_node *n = expr->left; n; n = n->next) {
        output_expr(n, false);
        if (n->next) {
          printf(", ");
        }
      }
      printf(")");
      break;
    case ast_if:
      printf("if (");
      output_expr(expr->val.node, false);
      printf(") ");
      if (returns) {
        printf("return ");
      }
      output_expr(expr->left, false);
      if (returns) {
        printf("; return ");
      } else {
        printf("; else ");
      }
      output_expr(expr->right, false);
  }
  if (returns) {
    if (expr->type == ast_if) {
      printf(";\n    return 0;");
    }
    printf(";\n");
  }
}

void output_def(ast_node *def) {
  if (!def) {
    return;
  }
  id_list *lp = def->left->val.identifiers;
  printf("  function %s(", lp->id);
  for (lp = lp->next; lp; lp = lp->next) {
    printf("%s", lp->id);
    if (lp->next) {
      printf(", ");
    }
  }
  printf(") {\n");
  for (lp = def->left->val.identifiers->next; lp; lp = lp->next) {
    printf("    %s = +%s;\n", lp->id, lp->id);
  }
  printf("    ");
  output_expr(def->right, true);
  printf("  }\n");
  output_def(def->next);
}

bool output_extern(id_list *l) {
  bool use_foreign = false;
  if (l) {
    if (((l->type & id_used) == id_used) &&
        ((l->type & id_extern) == id_extern)) {
      printf("  var %s = ", l->id);
      if ((l->type & id_stdlib) == id_stdlib) {
        printf("stdlib.");
        if ((l->type & id_math) == id_math) {
          printf("Math.");
        }
      } else {
        printf("foreign.");
        use_foreign = true;
      }
      printf("%s;\n", l->id);
    }
    use_foreign = output_extern(l->next) || use_foreign;
  }
  return use_foreign;
}

void output_main(ast_node *n) {
  printf("  function $() {\n");
  for (; n; n = n->next) {
    printf("    ");
    if (!n->next) {
      printf("return ");
    }
    output_expr(n, false);
    printf(";\n");
  }
  printf("  }\n");
}

ast_node *parse(void) {
  ast_node *n;
  id_list *l;
  ast_node *p = (ast_node *)malloc(sizeof(ast_node));
  p->val.identifiers = NULL;
  int mask = id_extern | id_stdlib;
  p->val.identifiers = prepend_id(p->val.identifiers, "Infinity", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "NaN", mask);
  mask |= id_math;
  p->val.identifiers = prepend_id(p->val.identifiers, "E", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "LN10", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "LN2", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "LOG2E", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "LOG10E", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "PI", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "SQRT1_2", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "SQRT2", mask);
  mask |= id_function;
  p->val.identifiers = prepend_id(p->val.identifiers, "acos", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "asin", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "atan", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "cos", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "sin", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "tan", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "ceil", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "floor", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "exp", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "log", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "sqrt", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "abs", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "atan2", mask);
  p->val.identifiers = prepend_id(p->val.identifiers, "pow", mask);
  get_next_token();
  while (cur_tok != tok_eof) {
    switch (cur_tok) {
      case ';':
        get_next_token();
        break;
      case tok_def:
        n = parse_definition(p);
        n->next = p->left;
        p->left = n;
        p->val.identifiers = prepend_id(p->val.identifiers,
            n->left->val.identifiers->id, id_function);
        break;
      case tok_extern:
        n = parse_extern();
        l = lookup(p->val.identifiers, n->val.identifiers->id);
        if (!l) {
          p->val.identifiers = prepend_id(p->val.identifiers,
              n->val.identifiers->id, id_extern | id_function);
        }
        break;
      default:
        n = parse_expression(p);
        if (p->next) {
          p->next->next = n;
        } else {
          p->right = n;
        }
        p->next = n;
    }
  }
  return p;
}

void output(ast_node *p) {
  printf("console.log((function (stdlib, foreign) {\n");
  printf("  \"use asm\";\n");
  bool use_foreign = output_extern(p->val.identifiers);
  output_def(p->left);
  output_main(p->right);
  printf("  return $;\n");
  printf("}(this");
  if (use_foreign) {
    printf(", foreign");
  }
  printf("))());\n");
}

int main(void) {
  output(parse());
  return 0;
}
