#ifndef LEXER_H
#define LEXER_H

#include "Tokens.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

class Lexer {
public:
   inline explicit Lexer(const std::string& src) 
      : m_src(std::move(src)) {}
   std::vector<Token> tokenize();

private:
   [[nodiscard]] std::optional<char> peek(int) const;
   bool peek_eval(char, int);
   inline char consume() { return m_src.at(m_currIndex++); }
   inline Token resolveSymbol(char);

   const std::string m_src;
   size_t m_currIndex = 0;
};


#endif // LEXER_H