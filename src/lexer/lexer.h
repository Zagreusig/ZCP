#ifndef LEXER_H
#define LEXER_H

#include <optional>
#include <string>
#include <vector>

enum class TokenType {
   NONE,
   _EXIT,
   RETURN,
   IF,
   ELSE,
   WHEN,
   FOR,
   WHILE,
   INT_LIT,
   CHAR_LIT,
   INT,
   IDENTIFIER,
   HAVE,
   COLON,
   SEMICOLON,
   DOUBLE_QUOTE,
   APOSTRAPHE,
   FULL_STOP,
   COMMA,
   HUH,
   OPERATOR_BANG,
   OPEN_PAREN,
   OPEN_BRACE,
   OPEN_BRACKET,
   CLOSE_PAREN,
   CLOSE_BRACE,
   CLOSE_BRACKET,
   OPERATOR_EQUALS,
   OPERATOR_PLUS,
   OPERATOR_ASTERISK,
   OPERATOR_SLASH,
   OPERATOR_DASH,
   OPERATOR_CARET,
   OPERATOR_PERCENT,
   OPERATOR_LT,
   OPERATOR_GT,
   OPERATOR_EQUAL_EQUAL,
   OPERATOR_NOT_EQUAL,
   OPERATOR_LESS_EQUAL,
   OPERATOR_GREATER_EQUAL,
};

struct Token {
   TokenType type                   = TokenType::NONE;
   std::optional<std::string> value = "";
};


class Lexer {
public:
   inline explicit Lexer(const std::string& src) 
      : m_src(std::move(src)) {}
   std::vector<Token> tokenize();

private:
   [[nodiscard]] std::optional<char> peek(int) const;
   inline char consume() { return m_src.at(m_currIndex++); }
   inline Token resolveSymbol(char);

   const std::string m_src;
   size_t m_currIndex = 0;
};


#endif // LEXER_H