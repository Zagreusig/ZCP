#include "lexer.h"
#include "driver/compiler.h"
#include "symbols/EscapeChars.h"
#include <iostream>
#include <unordered_map>

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
   { "is", TokenType::IS }, { "kinda", TokenType::KINDA }, // ignore these for now - I might not use them
   { "print", TokenType::PRINT }, { "println", TokenType::PRINTLN },
   { "readc", TokenType::READC }, { "readln", TokenType::READS }, { "reads", TokenType::READS},
   { "readi", TokenType::READI }, { "readf", TokenType::READF },
   { "global", TokenType::GLOBAL }, { "const", TokenType::CONST }
};


[[nodiscard]] std::optional<char> Lexer::peek(int offset = 0) const {
   if (m_currIndex + offset >= m_src.length()) return {};
   else return m_src.at(m_currIndex + offset);
} 


bool Lexer::peek_eval(char ch, int offset = 0) {
   return peek(offset).has_value() && peek(offset).value() == ch;
}


std::vector<Token> Lexer::lex() {
   try {
      return tokenize();
   } catch (const CompilerError& e) {
      std::cerr << "At " << e.line() << ":" << e.col() << ": " << e.what() << std::endl;
      return tokens;
   }
}


std::vector<Token> Lexer::tokenize() {
   std::string buf {};
   // std::vector<Token> tokens {};
   m_src = m_ctx.get_src();

   while (peek().has_value()) {
      if (!tokens.empty() && tokens.back().type == TokenType::COMMENT) {
         while (!peek_eval('\n')) consume();
         consume(); // '\n'
         tokens.pop_back();
      }


      if (std::isalpha(peek().value())) { 
         int tok_line = m_line, tok_col = m_col;        
         buf.push_back(consume());
         while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_'))
            buf.push_back(consume());
         
         auto it = KEYWORDS.find(buf);
         if (it != KEYWORDS.end())
            tokens.push_back({ .type = it->second, .line = tok_line, .col = tok_col });
         else
            tokens.push_back({ .type = TokenType::IDENTIFIER, .value = buf,
                               .line = tok_line, .col = tok_col });
         
         buf.clear(); continue;
      }
      else if (std::isspace(peek().value())) { consume(); continue; }
      else if (std::isdigit(peek().value())) {
         int tok_line = m_line, tok_col = m_col;
         buf.push_back(consume());
         while (peek().has_value() && std::isdigit(peek().value()))
            buf.push_back(consume());
         tokens.push_back({.type = TokenType::INT_LIT, .value = buf,
                           .line = tok_line, .col = tok_col });
         buf.clear(); continue;
      }
      else if (peek().value() == '\'') {
         int tok_line = m_line, tok_col = m_col;
         consume();
         if (!peek().has_value())
            m_ctx.diag.fatal(CompPhase::Lexing, tok_line, tok_col, "Expected char literal.");
         char c = consume();
         if (c == '\\') {
            if (!peek().has_value()) { 
               m_ctx.diag.error(CompPhase::Lexing, tok_line, tok_col, "Unterminated escape.");
               break; 
            }
            c = Esc::translate_escape(consume());
         }
         if (!peek().has_value() || peek().value() != '\'')
            m_ctx.diag.fatal(CompPhase::Lexing, tok_line, tok_col, "Expected closing '.");
         consume();
         tokens.push_back({ .type = TokenType::CHAR_LIT, .value = std::string(1, c),
                            .line = tok_line, .col = tok_col });
         continue;
      }
      else if (peek().value() == '"') {
         int tok_line = m_line, tok_col = m_col;
         consume();
         while (peek().has_value() && peek().value() != '"') {
            char c = consume();
            if (c == '\\') {
               if (!peek().has_value()) { m_ctx.diag.error(CompPhase::Lexing, tok_line, tok_col, "Unterminated escape."); break; }
               char esc = consume(), translated = Esc::translate_escape(esc);
               if (esc == translated) { buf.push_back(esc); }
               else { buf.push_back(translated); }
            } else {
               buf.push_back(c);
            }
         }
         if (!peek().has_value()) 
            m_ctx.diag.fatal(CompPhase::Lexing, tok_line, tok_col, "Expected closing \"");
         if (peek().value() == '"' && buf.length() < 1) buf.push_back('\0'); 

         consume();
         tokens.push_back({ .type = TokenType::STR_LIT, .value = buf,
                            .line = tok_line, .col = tok_col });
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
   case  ';': return Token { .type = TokenType::SEMICOLON         };
   case  ':': return Token { .type = TokenType::COLON             };
   case  '?': return Token { .type = TokenType::QUESTION          };
   case  '!': 
      if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_NOT_EQUAL     };
      }
      return Token { .type = TokenType::OPERATOR_BANG             };
   case  '.': return Token { .type = TokenType::FULL_STOP         };
   case  ',': return Token { .type = TokenType::COMMA             };
   case '\'': return Token { .type = TokenType::APOSTRAPHE        };
   case '\"': return Token { .type = TokenType::DOUBLE_QUOTE      };
   case  '(': return Token { .type = TokenType::OPEN_PAREN        };
   case  ')': return Token { .type = TokenType::CLOSE_PAREN       };
   case  '[': return Token { .type = TokenType::OPEN_BRACKET      };
   case  ']': return Token { .type = TokenType::CLOSE_BRACKET     };
   case  '{': return Token { .type = TokenType::OPEN_BRACE        };
   case  '}': return Token { .type = TokenType::CLOSE_BRACE       };
   case  '=': 
      if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_EQUAL_EQUAL   };
      }
      return Token { .type = TokenType::OPERATOR_EQUALS           };
   
      case  '+': 
      if (peek_eval('+')) {
         consume();
         return Token { .type = TokenType::OPERATOR_INCR          };
      }
      else if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_ADD_EQ        };
      }
      return Token { .type = TokenType::OPERATOR_PLUS             };
   
      case  '*':
      if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_MUL_EQ        };
      }
      else if (peek_eval('/')) {
         consume();
         return Token { .type = TokenType::END_COMMENT_BLOCK      };
      }
      return Token { .type = TokenType::OPERATOR_ASTERISK         };
   
      case  '/':
      if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_DIV_EQ        };
      }
      else if (peek_eval('/')) {
         consume();
         return Token { .type = TokenType::COMMENT                };
      }
      else if (peek_eval('*')) {
         consume();
         return Token { .type = TokenType::START_COMMENT_BLOCK    };
      }
      return Token { .type = TokenType::OPERATOR_SLASH            };
   
   case  '%': return Token { .type = TokenType::OPERATOR_PERCENT  };
   
   case  '-': 
      if (peek_eval('>')) {
         consume();
         return Token { .type = TokenType::OPERATOR_ARROW         };
      }
      else if (peek_eval('-')) {
         consume();
         return Token { .type = TokenType::OPERATOR_DECR          };
      }
      else if (peek_eval('=')) {
         consume();
         return Token {.type = TokenType::OPERATOR_SUB_EQ         };
      }
      return Token { .type = TokenType::OPERATOR_DASH             };
   
   case  '^': return Token { .type = TokenType::OPERATOR_CARET    };
   
   case  '>': 
      if (peek_eval('=')) {
         consume();
         return Token { .type = TokenType::OPERATOR_GREATER_EQUAL };
      }
      return Token { .type = TokenType::OPERATOR_GT               };
   
   case  '<': 
      if (peek_eval('=') ) {
         consume();
         return Token { .type = TokenType::OPERATOR_LESS_EQUAL    };
      }
      return Token { .type = TokenType::OPERATOR_LT               };
   
   case  '|':
      if (peek_eval('|')) {
         consume();
         return Token { .type = TokenType::OPERATOR_LOGICAL_OR    };
      }
      return Token { .type = TokenType::PIPE                      };
   
   case '&':
      if (peek_eval('&')) {
         consume();
         return Token { .type = TokenType::OPERATOR_LOGICAL_AND   };
      }
      return Token { .type = TokenType::AMPERSAND                 };
   default:  m_ctx.diag.error(CompPhase::Lexing, m_line, m_col, "Unknown symbol."); return {};
   }
}

