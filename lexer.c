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

char tok_string[ID_STRING_SIZE];
double num_val;

int gettok(void);

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

int main() {
  for (int tok = gettok(); tok != tok_eof; tok = gettok()) {
    if (tok == tok_def) {
      printf("DEF\n");
    } else if (tok == tok_extern) {
      printf("EXTERN\n");
    } else if (tok == tok_identifier) {
      printf("ID<%s>\n", tok_string);
    } else if (tok == tok_number) {
      printf("NUMBER<%g>\n", num_val);
    } else if (tok >= ' ' && tok <= '~'){
      printf("'%c'\n", (char) tok);
    } else {
      printf("CHARACTER<%d>\n", tok);
    }
  }
  return 0;
}
