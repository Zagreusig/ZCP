#include "lexer.h"

#include <iostream>
#include <unordered_map>
#include <cctype>
#include <stdexcept>
#include <utility>

#include "driver/compiler.h"
#include "Core/EscapeChars.h"
#include "ErrorHandler.h"
#include "TokenTable.h"
#include "Tokens.h"
#include "phase.h"

const std::unordered_map<std::string, TokenType> KEYWORDS = {
   { "exit", TokenType::_EXIT }, { "return", TokenType::RETURN },
   { "func", TokenType::FUNC }, { "fn", TokenType::FUNC },
   { "if", TokenType::IF }, { "else", TokenType::ELSE },
   { "while", TokenType::WHILE }, { "for", TokenType::FOR },
   { "have", TokenType::HAVE },
   { "void", TokenType::VOID },
   { "int", TokenType::INT }, { "char", TokenType::CHAR }, { "bool", TokenType::BOOL }, { "str", TokenType::STR },
   { "float", TokenType::NONE },
   { "and", TokenType::OPERATOR_LOGICAL_AND }, { "or", TokenType::OPERATOR_LOGICAL_OR },
   { "print", TokenType::PRINT }, { "println", TokenType::PRINTLN },
   { "readc", TokenType::READC }, { "readln", TokenType::READS }, { "reads", TokenType::READS},
   { "readi", TokenType::READI }, { "readf", TokenType::READF },
   { "global", TokenType::GLOBAL }, { "const", TokenType::CONST },
   { "true", TokenType::TRUE }, { "false", TokenType::FALSE }
};


std::vector<Token> Lexer::lex() {
   try {
      return tokenize();
   } catch (const CompilerError& e) {
      std::cerr << "At " << e.line() << ":" << e.col() << ": " << e.what() << std::endl;
      return tokens;
   }
}



[[nodiscard]] std::optional<char> Lexer::peek(int offset = 0) const {
   if (m_currIndex + offset >= m_src.length()) return {};
   else return m_src.at(m_currIndex + offset);
} 


bool Lexer::peek_eval(char ch, int offset = 0) {
   return peek(offset).has_value() && peek(offset).value() == ch;
}


std::vector<Token> Lexer::tokenize() {
   std::string buf {};

   while (peek().has_value()) {
      if (!tokens.empty() && tokens.back().type == TokenType::COMMENT) {
         while (!peek_eval('\n')) consume();
         consume(); // '\n'
         tokens.pop_back();
      }
      else if (!tokens.empty() && tokens.back().type == TokenType::START_COMMENT_BLOCK) {
         tokens.pop_back();
         while(peek().has_value()) {
            if (peek_eval('*') && peek_eval('/', 1)) {
               consume(); consume(); break;
            }
            consume();
         }
         /** TODO: make this not like this, it's gross nasty */
         if (!peek().has_value() && peek_eval('/', -2)) {
            m_compiler.diagnostics.error(CompPhase::Lexing, m_compiler.current_filename(), m_line, m_col,
                                         "Unterminated comment block.");
         }
      }
      else if (std::isalpha(peek().value())) { 
         int tok_line = m_line, tok_col = m_col;        
         buf.push_back(consume());
         while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_'))
            buf.push_back(consume());
         
         auto it = KEYWORDS.find(buf);
         if (it != KEYWORDS.end())
            tokens.push_back(tok::make(it->second, m_file_id, tok_line, tok_col));
         else
            tokens.push_back(tok::make_ident(buf, m_file_id, tok_line, tok_col));
         buf.clear(); continue;
      }
      else if (std::isspace(peek().value())) { consume(); continue; }
      else if (std::isdigit(peek().value())) {
         int tok_line = m_line, tok_col = m_col;
         buf.push_back(consume());
         while (peek().has_value() && std::isdigit(peek().value()))
            buf.push_back(consume());
         try {
            tokens.push_back(tok::make_int(std::stoll(buf), m_file_id, tok_line, tok_col));
         } catch (const std::out_of_range&) {
            m_compiler.diagnostics.fatal(CompPhase::Lexing, m_compiler.filename_by_id(m_file_id), tok_line, tok_col, "Int literal too large.");
         }
         
         buf.clear(); continue;
      }
      else if (peek().value() == '\'') {
         int tok_line = m_line, tok_col = m_col;
         consume();
         if (!peek().has_value())
            m_compiler.fatal(CompPhase::Lexing, m_file_id, tok_line, tok_col, "Expected char literal.");
         char c = consume();
         if (c == '\\') {
            if (!peek().has_value()) { 
               m_compiler.error(CompPhase::Lexing, m_file_id, tok_line, tok_col, "Unterminated escape.");
               break; 
            }
            c = Esc::translate_escape(consume());
         }
         if (!peek().has_value() || peek().value() != '\'')
            m_compiler.diagnostics.fatal(CompPhase::Lexing, m_compiler.filename_by_id(m_file_id), tok_line, tok_col, "Expected closing '.");
         consume();
         tokens.push_back(tok::make_char(c, m_file_id, tok_line, tok_col));
         continue;
      }
      else if (peek().value() == '"') {
         int tok_line = m_line, tok_col = m_col;
         consume();
         while (peek().has_value() && peek().value() != '"') {
            char c = consume();
            if (c == '\\') {
               if (!peek().has_value()) { m_compiler.error(CompPhase::Lexing, m_file_id, tok_line, tok_col, "Unterminated escape."); break; }
               char esc = consume(), translated = Esc::translate_escape(esc);
               if (esc == translated) { buf.push_back(esc); }
               else { buf.push_back(translated); }
            } else {
               buf.push_back(c);
            }
         }
         if (!peek().has_value()) 
            m_compiler.fatal(CompPhase::Lexing, m_file_id, tok_line, tok_col, "Expected closing \"");
         if (peek().value() == '"' && buf.length() < 1) buf.push_back('\0'); 

         consume();
         tokens.push_back(tok::make_str(buf, m_file_id, tok_line, tok_col));
         buf.clear(); continue;
      }
      else {
         int tok_line = m_line, tok_col = m_col;
         Token t = resolveSymbol(consume());
         t.line = tok_line; t.col = tok_col;
         tokens.push_back(t);
      }
   }
   m_currIndex = 0;
   return tokens;
}


inline Token Lexer::resolveSymbol(char symbol) {
   switch (symbol) {
   case  ';': return tok::make(TokenType::SEMICOLON, m_file_id, m_line, m_col); 
   case  ':': return tok::make(TokenType::COLON,     m_file_id, m_line, m_col); 
   case  '?': return tok::make(TokenType::QUESTION,  m_file_id, m_line, m_col); 
   case  '!': 
      if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_NOT_EQUAL, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_BANG,         m_file_id, m_line, m_col); 
   case  '.': return tok::make(TokenType::FULL_STOP,     m_file_id, m_line, m_col); 
   case  ',': return tok::make(TokenType::COMMA,         m_file_id, m_line, m_col); 
   case '\'': return tok::make(TokenType::APOSTRAPHE,    m_file_id, m_line, m_col); 
   case '\"': return tok::make(TokenType::DOUBLE_QUOTE,  m_file_id, m_line, m_col); 
   case  '(': return tok::make(TokenType::OPEN_PAREN,    m_file_id, m_line, m_col); 
   case  ')': return tok::make(TokenType::CLOSE_PAREN,   m_file_id, m_line, m_col); 
   case  '[': return tok::make(TokenType::OPEN_BRACKET,  m_file_id, m_line, m_col); 
   case  ']': return tok::make(TokenType::CLOSE_BRACKET, m_file_id, m_line, m_col); 
   case  '{': return tok::make(TokenType::OPEN_BRACE,    m_file_id, m_line, m_col); 
   case  '}': return tok::make(TokenType::CLOSE_BRACE,   m_file_id, m_line, m_col); 
   case  '=': 
      if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_EQUAL_EQUAL, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_EQUALS, m_file_id, m_line, m_col); 
   
      case  '+': 
      if (peek_eval('+')) {
         consume();
         return tok::make(TokenType::OPERATOR_INCR, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_ADD_EQ, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_PLUS, m_file_id, m_line, m_col); 
   
      case  '*':
      if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_MUL_EQ, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('/')) {
         consume();
         return tok::make(TokenType::END_COMMENT_BLOCK, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_ASTERISK, m_file_id, m_line, m_col); 
   
      case  '/':
      if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_DIV_EQ, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('/')) {
         consume();
         return tok::make(TokenType::COMMENT, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('*')) {
         consume();
         return tok::make(TokenType::START_COMMENT_BLOCK, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_ASTERISK, m_file_id, m_line, m_col); 
   
   case  '%': return tok::make(TokenType::OPERATOR_PERCENT, m_file_id, m_line, m_col); 
   
   case  '-': 
      if (peek_eval('>')) {
         consume();
         return tok::make(TokenType::OPERATOR_ARROW, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('-')) {
         consume();
         return tok::make(TokenType::OPERATOR_DECR, m_file_id, m_line, m_col); 
      }
      else if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_SUB_EQ, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_DASH, m_file_id, m_line, m_col); 
   
   case  '^': return tok::make(TokenType::OPERATOR_CARET, m_file_id, m_line, m_col); 
   
   case  '>': 
      if (peek_eval('=')) {
         consume();
         return tok::make(TokenType::OPERATOR_GREATER_EQUAL, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_GT, m_file_id, m_line, m_col); 
   
   case  '<': 
      if (peek_eval('=') ) {
         consume();
         return tok::make(TokenType::OPERATOR_LESS_EQUAL, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::OPERATOR_LT, m_file_id, m_line, m_col); 
   
   case  '|':
      if (peek_eval('|')) {
         consume();
         return tok::make(TokenType::OPERATOR_LOGICAL_OR, m_file_id, m_line, m_col); 
      }
      return tok::make(TokenType::PIPE, m_file_id, m_line, m_col); 
   
   case '&':
      if (peek_eval('&')) {
         consume();
         return tok::make(TokenType::OPERATOR_LOGICAL_AND, m_file_id, m_line, m_col); 
      } 
      return tok::make(TokenType::AMPERSAND, m_file_id, m_line, m_col); 
   case '#': return tok::make(TokenType::POUND, m_file_id, m_line, m_col); 
   default:  m_compiler.error(CompPhase::Lexing, m_file_id, m_line, m_col, "Unknown symbol."); 
             return tok::make(TokenType::INVALID, m_file_id, m_line, m_col); 
   }
}

