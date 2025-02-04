#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

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
  }

  // Otherwise, just return the character as its ASCII value.
  int this_char = last_char;
  last_char = getchar();
  return this_char;
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


//=========================
// Logger
//=========================

std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}


//=========================
// Parser
//=========================

static int curr_token;
static int getNextToken() { return curr_token = gettok(); }

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto result = std::make_unique<NumberExprAST>(num_val);
  getNextToken(); // Eat the number
  return std::move(result);
}

static std::unique_ptr<ExprAST> ParseExpression();

// Parses an expression starting with an open bracket '('.
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // Eat '('
  auto V = ParseExpression();
  if (!V) { return nullptr; }

  if (curr_token != ')') { return LogError("expected ')'"); };
  getNextToken(); // Eat ')'
  return V;
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string id_name = identifier_str;

  getNextToken();  // Eat identifier
  if (curr_token != '(') { return std::make_unique<VariableExprAST>(id_name); }
  
  getNextToken();  // Eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;
  // If the function has arguments, parse the arguments.
  if (curr_token != ')') {
    while (true) {
      if (auto Arg = ParseExpression()) {
        Args.push_back(std::move(Arg));
      } else { return nullptr; }

      if (curr_token == ')')
        break;

      if (curr_token != ',') { return LogError("Expected ')' or ',' in argument list"); }
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return std::make_unique<CallExprAST>(id_name, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (curr_token) {
  default:
    return LogError("Unknown token when expecting an expression");
  case token_identifier:
    return ParseIdentifierExpr();
  case token_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  }
}

// An unordered map that determines the precedence of binary operators (i.e. like BEDMAS) - higher 
// precendence means that operator will be processed first.
static std::unordered_map<char, int> BinopPrecedence;

/// Get the precedence of the pending binary operator token.
static int GetTokenPrecedence() {
  if (!isascii(curr_token)) { return -1; }

  // Make sure it's a declared binop.
  int token_precedence = BinopPrecedence[curr_token];
  if (token_precedence <= 0) {
    return -1;
  } else {
    return token_precedence;
  }
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int expression_precedence, std::unique_ptr<ExprAST> LHS) {
  // If this is a binary operator, find its precedence.
  while (true) {
    int token_precedence = GetTokenPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (token_precedence < expression_precedence) { return LHS; }

    int binary_operator = curr_token;
    getNextToken();  // Eat binary operator

    // Parse the primary expression after the binary operator.
    auto RHS = ParsePrimary();
    if (!RHS) { return nullptr; }

    int next_precedence = GetTokenPrecedence();
    if (token_precedence < next_precedence) {
      RHS = ParseBinOpRHS(token_precedence + 1, std::move(RHS));
      if (!RHS) { return nullptr; }
    }
    
    // Merge LHS/RHS.
    LHS = std::make_unique<BinaryExprAST>(binary_operator, std::move(LHS), std::move(RHS));
  }
}

// Parse function header.
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (curr_token != token_identifier) { return LogErrorP("Expected function name in prototype"); }

  std::string func_name = identifier_str;
  getNextToken();

  if (curr_token != '(') { return LogErrorP("Expected '(' in prototype"); }

  // Read the list of argument names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == token_identifier) { ArgNames.push_back(identifier_str); }
  if (curr_token != ')') { return LogErrorP("Expected ')' in prototype"); }

  getNextToken();  // Eat ')'

  return std::make_unique<PrototypeAST>(func_name, std::move(ArgNames));
}

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

// Parse function definition.
static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();  // Eat 'def'
  auto Prototype = ParsePrototype();
  if (!Prototype) { return nullptr; }

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Prototype), std::move(E));
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken();  // Eat 'extern'
  return ParsePrototype();
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous prototype.
    auto Prototype = std::make_unique<PrototypeAST>("", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Prototype), std::move(E));
  }
  return nullptr;
}

static void HandleDefinition() {
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}


//=========================
// Driver
//=========================

static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (curr_token) {
    case token_eof:
      return;
    case ';': // Ignore top-level semicolons
      getNextToken();
      break;
    case token_def:
      HandleDefinition();
      break;
    case token_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

int main() {
  // Set standard binary operators.
  BinopPrecedence['<'] = 100;
  BinopPrecedence['+'] = 200;
  BinopPrecedence['-'] = 200;
  BinopPrecedence['*'] = 300;

  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

  return 0;
}