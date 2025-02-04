#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//=========================
// Lexer
//=========================

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  token_eof = -1,        // End of file
  token_def = -2,        // Define
  token_extern = -3,     // External function
  token_identifier = -4, // Keyword
  token_number = -5,     // Numeric data
};

static std::string identifier_str; // Filled in if token_identifier
static double num_val;             // Filled in if token_number

/// gettok - Return the next token from standard input.
static int gettok() {
  static char last_char = ' ';

  // Skip any whitespace.
  while (isspace(last_char)) {
    last_char = getchar();
  }

  if (isalpha(last_char)) { // Regular expression: [a-zA-Z][a-zA-Z0-9]*
    identifier_str = last_char;
    while (isalnum((last_char = getchar())))
      identifier_str += last_char;

    if (identifier_str == "def") {
      return token_def;
    } else if (identifier_str == "extern") {
      return token_extern;
    }
    // If the identifier is not a keyword, it is a generic identifier.
    return token_identifier;
  } else if (isdigit(last_char) || last_char == '.') {   // Regular expression: [0-9.]+
    std::string num_str;
    do {
      num_str += last_char;
      last_char = getchar();
    } while (isdigit(last_char) || last_char == '.');

    num_val = strtod(num_str.c_str(), 0);
    return token_number;
  } else if (last_char == '#') {
    // Comment until end of line.
    do {
      last_char = getchar();
    } while (last_char != EOF && last_char != '\n' && last_char != '\r');

    if (last_char != EOF) {
      return gettok();
    }
  } else if (last_char == EOF) {
    // Check for end of file.  Don't eat the EOF.
    return token_eof;
  } else {
    // Otherwise, just return the character as its ascii value.
    int this_char = last_char;
    last_char = getchar();
    return this_char;
  }
}

//=========================
// Abstract Syntax Tree
//=========================
// Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
};

// Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};

// Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
};

// Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op; // A character representing the operator, e.g. '+'.
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

// Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
};

// This class represents the "prototype" for a function, which captures its name,
// and its argument names (thus implicitly the number of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
};

/// This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Prototype;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Prototype,
              std::unique_ptr<ExprAST> Body)
    : Prototype(std::move(Prototype)), Body(std::move(Body)) {}
};



