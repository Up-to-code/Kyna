#include "kyna/lexing/legacy_lexer.hpp"
#include <cassert>

int main() {
  auto tokens = kyna::lex("// comment\nvar values = [1, 2];");
  assert(tokens[0].kind == kyna::TokenKind::Var);
  assert(tokens[3].kind == kyna::TokenKind::LeftBracket);
  assert(tokens.back().kind == kyna::TokenKind::End);
}
