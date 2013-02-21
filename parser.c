#include <ctype.h>
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
  tok_number = -5       // number (see num_val)
};

// AST node types
enum {
  ast_number = 0,  // val = double
  ast_var,         // val = string
  ast_binary,      // val = op, left, right
  ast_call,        // val = string (callee), next = first arg
  ast_prototype,   // val = identifiers (name, ...)
  ast_function     // left = proto (ast_prototype), right = body
};

#pragma clang diagnostic ignored "-Wpadded"

typedef struct id_list {
  char *id;
  struct id_list *next;
} id_list;

typedef struct ast_node {
  int type;
  union {
    int op;
    double number;
    char *string;
    id_list *identifiers;
  } val;
  struct ast_node *left;
  struct ast_node *right;
  struct ast_node *next;
} ast_node;


int gettok(void);
int get_next_token(void);
ast_node *parse_expression(void);
ast_node *error(const char *);
ast_node *parse_number_expr(void);
ast_node *parse_paren_expr(void);
ast_node *parse_identifier_expr(void);
ast_node *parse_primary(void);
int get_tok_precedence(void);
ast_node *parse_expression(void);
ast_node *parse_binop_rhs(int, ast_node *);
ast_node *parse_prototype(void);
ast_node *parse_definition(void);
ast_node *parse_extern(void);
ast_node *parse_top_level_expr(void);
void handle_definition(void);
void handle_extern(void);
void handle_top_level_expression(void);
void main_loop(void);


// Lexer

char tok_string[ID_STRING_SIZE];
double num_val;

int gettok() {
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

int get_next_token() {
  return cur_tok = gettok();
}

ast_node *error(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  return NULL;
}

// numberexpr ::= number
ast_node *parse_number_expr() {
  ast_node *result = (ast_node *)malloc(sizeof(ast_node));
  if (result) {
    result->type = ast_number;
    result->val.number = num_val;
  }
  get_next_token();
  return result;
}

// parenexpr ::= '(' expression ')'
ast_node *parse_paren_expr() {
  get_next_token();
  ast_node *v = parse_expression();
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
ast_node *parse_identifier_expr() {
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->val.string = strdup(tok_string);
  get_next_token();
  if (cur_tok != '(') {
    n->type = ast_var;
    return n;
  }
  get_next_token();
  n->type = ast_call;
  ast_node *argp = n;
  if (cur_tok != ')') {
    while (1) {
      ast_node *arg = parse_expression();
      if (!arg) {
        return NULL;
      }
      argp->next = arg;
      argp = argp->next;
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

// primary ::= identifierexpr
//         ::= numberexpr
//         ::= parenexpr
ast_node *parse_primary() {
  switch (cur_tok) {
    case tok_identifier: return parse_identifier_expr();
    case tok_number: return parse_number_expr();
    case '(': return parse_paren_expr();
    default: return error("unknown token when expecting an expression");
  }
}

int get_tok_precedence() {
  switch (cur_tok) {
    case '<': return 10;
    case '+': return 20;
    case '-': return 20;
    case '*': return 40;
    default:  return -1;
  }
}

// expression ::= primary binoprhs
ast_node *parse_expression() {
  ast_node *lhs = parse_primary();
  return lhs ? parse_binop_rhs(0, lhs) : NULL;
}

// binoprhs ::= ('+' primary)*
ast_node *parse_binop_rhs(int expr_prec, ast_node *lhs) {
  while (1) {
    int tok_prec = get_tok_precedence();
    if (tok_prec < expr_prec) {
      return lhs;
    }
    int bin_op = cur_tok;
    get_next_token();
    ast_node *rhs = parse_primary();
    if (!rhs) {
      return NULL;
    }
    int next_prec = get_tok_precedence();
    if (tok_prec < next_prec) {
      rhs = parse_binop_rhs(tok_prec + 1, rhs);
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
ast_node *parse_prototype() {
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
ast_node *parse_definition() {
  get_next_token();
  ast_node *proto = parse_prototype();
  if (!proto) {
    return NULL;
  }
  ast_node *e = parse_expression();
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

// top_level_expression ::= expression
ast_node *parse_top_level_expr() {
  ast_node *e = parse_expression();
  if (!e) {
    return NULL;
  }
  // Make an anonymous prototype (no identifier at all)
  ast_node *proto = (ast_node *)malloc(sizeof(ast_node));
  proto->type = ast_prototype;
  proto->val.identifiers = NULL;
  ast_node *n = (ast_node *)malloc(sizeof(ast_node));
  n->type = ast_function;
  n->left = proto;
  n->right = e;
  return n;
}

void handle_definition() {
  if (parse_definition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    get_next_token();
  }
}

void handle_extern() {
  if (parse_extern()) {
    fprintf(stderr, "Parsed an extern.\n");
  } else {
    get_next_token();
  }
}

void handle_top_level_expression() {
  if (parse_top_level_expr()) {
    fprintf(stderr, "Parsed a top-level expression.\n");
  } else {
    get_next_token();
  }
}

void main_loop() {
  while (1) {
    fprintf(stderr, "ready> ");
    switch (cur_tok) {
      case tok_eof: return;
      case ';': get_next_token(); break;
      case tok_def: handle_definition(); break;
      case tok_extern: handle_extern(); break;
      default: handle_top_level_expression(); break;
    }
  }
}

int main() {
  fprintf(stderr, "ready> ");
  get_next_token();
  main_loop();
  return 0;
}
