#ifndef TOKENS_H
#define TOKENS_H

#include "TokenTable.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>

// ===========================================================================
// Tokens.h
//
// A token carries a type, a variant payload, and a source location.
//
// The payload is an std::variant:
//    - std::monostate : no value (operators, punctuation, most keywords)
//    - int64_t        : INT_LIT  (parsed to integer by lexer)
//    - char           : CHAR_LIT
//    - std::string    : STR_LIT as well as IDENTIFIER
//
// INVARIANT: the lexer establishes each token type carries its correct
// payload. A token with no value holds std::monostate. Downstream code reads
// the payload via the typed accessors below, which throw if the invariant is
// violated - i.e. a std::bad_variant_access means a compiler bug (lexer built
// the token wrong), NOT USER ERROR. User-facing semantic errors live in the
// analyzer class and go through diagnostics, never through these throws.
// ===========================================================================

using TokenValue = std::variant<
   std::monostate,    // no value
   int64_t,           // int literals
   char,              // char literals
   std::string        // string literals & identifiers
>;

struct Token {
   TokenType type    = TokenType::NONE;
   TokenValue value;
   int line          = 0;
   int col           = 0;
   // FileID for multifile


   // ---------------------------------------------------------------------------
   // Typed accessors. These assert that the payload invariant: calling the wrong
   // one (e.g. int_val() on an identifier) throws std::bad_variant_access,
   // signalling a compiler-internal bug. Use has_* to check when unsure.
   // ---------------------------------------------------------------------------
   bool has_value() const { return !std::holds_alternative<std::monostate>(value); }

   bool is_int()  const { return std::holds_alternative<int64_t>(value); }
   bool is_char() const { return std::holds_alternative<char>(value); }
   bool is_text() const { return std::holds_alternative<std::string>(value); }

   int64_t int_val()  const  { return std::get<int64_t>(value); }
   char    char_val() const { return std::get<char>(value); }

   // Returns a reference to avoid copying the string.
   const std::string& text()     const { return std::get<std::string>(value); }

   // Non-throwing variants: return nullptr if payload isn't that type.
   const int64_t*     try_int()  const { return std::get_if<int64_t>(&value); }
   const char*        try_char() const { return std::get_if<char>(&value); }
   const std::string* try_text() const { return std::get_if<std::string>(&value); }

   // Convenience: token's spelling for diagnostics.
   //   - text tokens (identifiers / str lits) print their text.
   //   - everything else prints canonical name from table.
   std::string spelling() const {
      if (auto* s = try_text()) return *s;
      return std::string(to_string(type));
   }
};


// ---------------------------------------------------------------------------
// Construction helpers. Lexer uses these to create each type payload 
// invariant, all versions established here, and can't get messed up at 
// the call sites.
// ---------------------------------------------------------------------------
namespace tok {
   // Valueless token: ops, puncts, kwrds.
   inline Token make(TokenType type, int line, int col) {
      return Token{ type, std::monostate{}, line, col };
   }

   inline Token make_int(int64_t v, int line, int col) {
      return Token{ TokenType::INT_LIT, v, line, col };
   }

   inline Token make_char(char c, int line, int col) {
      return Token{ TokenType::CHAR_LIT, c, line, col };
   }

   inline Token make_str(std::string s, int line, int col) {
      return Token{ TokenType::STR_LIT, std::move(s), line, col };
   }

   inline Token make_ident(std::string name, int line, int col) {
      return Token{ TokenType::IDENTIFIER, std::move(name), line, col };
   }
} // namespace tok


#endif // TOKENS_H