#ifndef LEXER_H
#define LEXER_H

#include "ErrAndRep/ErrorHandler.h"
#include "Tokens.h"
#include <iostream>
#include <optional>
#include <string>
#include <vector>

class Compiler;

class Lexer {
public:
   inline explicit Lexer(Compiler& cmp) : m_ctx(cmp) {}
   std::vector<Token> lex();
   std::vector<Token> tokenize();

private:
   [[nodiscard]] std::optional<char> peek(int) const;
   bool peek_eval(char, int);
   inline char consume() { 
      char c = m_src.at(m_currIndex++); 
      if (c == '\n') { m_line++; m_col = 1; }
      else           { m_col++; }
      return c;
   }
   inline Token resolveSymbol(char);
   void synchronize();

   Compiler& m_ctx;

   std::vector<Token> tokens {};

   std::string m_src;
   size_t m_currIndex = 0;
   int m_line = 1, m_col = 1;
};


#endif // LEXER_H