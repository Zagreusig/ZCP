#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <optional>
#include <string>
#include <vector>

#include "Core/ErrorHandler.h"
#include "Core/Tokens.h"

class Lexer {
public:
   inline explicit Lexer(const std::string& source, const std::string& file_name, int file_id) 
      : m_src(source), m_file_id(file_id), m_file_name(file_name) {}
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

   std::string m_src;
   int m_file_id = 0;
   std::string m_file_name;

   std::vector<Token> tokens {};
   size_t m_currIndex = 0;
   int m_line = 1, m_col = 1;
};


#endif // LEXER_H