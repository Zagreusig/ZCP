#include "lexer.h"
#include <iostream>

[[nodiscard]] std::optional<char> Lexer::peek(int offset = 0) const {
   if (m_currIndex + offset >= m_src.length()) return {};
   else return m_src.at(m_currIndex + offset);
} 

std::vector<Token> Lexer::tokenize() {
   std::string buf {};
   std::vector<Token> tokens {};

   while (peek().has_value()) {
      if (std::isalpha(peek().value())) {         
         buf.push_back(consume());
         while (peek().has_value() && (std::isalnum(peek().value()) || peek().value() == '_'))
            buf.push_back(consume());
         if (buf == "exit") {
            tokens.push_back({.type = TokenType::_EXIT});
            buf.clear(); continue;
         }
         else if (buf == "return") {
            tokens.push_back({.type = TokenType::RETURN});
            buf.clear(); continue;
         }
         else if (buf == "func" || buf == "fn") {
            tokens.push_back({.type = TokenType::FUNC});
            buf.clear(); continue;
         }
         else if (buf == "if" || buf == "when") {
            tokens.push_back({.type = TokenType::IF});
            buf.clear(); continue;
         }
         else if (buf == "else") {
            tokens.push_back({.type = TokenType::ELSE});
            buf.clear(); continue;
         }
         else if (buf == "while") {
            tokens.push_back({.type = TokenType::WHILE});
            buf.clear(); continue;
         }
         else if (buf == "for") {
            tokens.push_back({.type = TokenType::FOR});
            buf.clear(); continue;
         }
         else if (buf == "have") {
            tokens.push_back({.type = TokenType::HAVE});
            buf.clear(); continue;
         }
         else if (buf == "int") {
            tokens.push_back({.type = TokenType::INT});
            buf.clear(); continue;
         }
         else if (buf == "char") {
            tokens.push_back({.type = TokenType::CHAR});
            buf.clear(); continue;
         }
         else if (buf == "and") {
            tokens.push_back({.type = TokenType::OPERATOR_LOGICAL_AND});
            buf.clear(); continue;
         }
         else if (buf == "or") {
            tokens.push_back({.type = TokenType::OPERATOR_LOGICAL_OR});
            buf.clear(); continue;
         }
         else if (buf == "print") {
            tokens.push_back({.type = TokenType::PRINT});
            buf.clear(); continue;
         }
         else if (buf == "println") {
            tokens.push_back({.type = TokenType::PRINTLN});
            buf.clear(); continue;
         }
         else {
            tokens.push_back({.type = TokenType::IDENTIFIER, .value = buf});
            buf.clear(); continue;
         }
      }
      else if (std::isdigit(peek().value())) {
         buf.push_back(consume());
         while (peek().has_value() && std::isdigit(peek().value()))
            buf.push_back(consume());
         tokens.push_back({.type = TokenType::INT_LIT, .value = buf});
         buf.clear(); continue;
      }
      else if (peek().value() == '\'') {
         consume();
         if (!peek().has_value()) {
            std::cerr << "Expected character literal." << std::endl;
            exit(EXIT_FAILURE);
         }

         char c = consume();
         if (!peek().has_value() || peek().value() != '\'') {
            std::cerr << "Expected closing '." << std::endl;
            exit(EXIT_FAILURE);
         }

         consume();
         tokens.push_back({ .type = TokenType::CHAR_LIT, .value = std::string(1, c) });
         continue;
      }
      else if (peek().value() == '\"') {
         consume();
         while ((!peek().has_value() || peek().value() != '\"') &&
                peek().value() != EOF) {
            buf.push_back(consume());
            /** TODO: Escape char logic */
         }
         
         if (!peek().has_value() || peek().value() != '\"') {
            std::cerr << "Expected closing \"." << std::endl;
            exit(EXIT_FAILURE);
         }
         
         consume();
         tokens.push_back({ .type = TokenType::STR_LIT, .value = buf });
         std::cout << "Str Lit parsed: " << tokens.back().value.value() << std::endl;
         buf.clear(); continue;
      }
      else if (std::isspace(peek().value())) { consume(); continue; }
      else
         tokens.push_back(resolveSymbol(consume()));
   }
   m_currIndex = 0;
   return tokens;
}


inline Token Lexer::resolveSymbol(char symbol) {
   switch (symbol) {
   case  ';': return Token { .type = TokenType::SEMICOLON         };
   case  ':': return Token { .type = TokenType::COLON             };
   case  '?': return Token { .type = TokenType::QUESTION               };
   case  '!': 
      if (peek().has_value() && peek().value() == '=') {
         consume();
         return Token { .type = TokenType::OPERATOR_NOT_EQUAL     };
      }
      return Token { .type = TokenType::OPERATOR_BANG };
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
      if (peek().has_value() && peek().value() == '=') {
         consume();
         return Token { .type = TokenType::OPERATOR_EQUAL_EQUAL   };
      }
      return Token { .type = TokenType::OPERATOR_EQUALS };
   case  '+': return Token { .type = TokenType::OPERATOR_PLUS     };
   case  '*': return Token { .type = TokenType::OPERATOR_ASTERISK };
   case  '/': return Token { .type = TokenType::OPERATOR_SLASH    };
   case  '%': return Token { .type = TokenType::OPERATOR_PERCENT  };
   case  '-': 
      if (peek().has_value() && peek().value() == '>') {
         consume();
         return Token { .type = TokenType::OPERATOR_ARROW         };
      }
      return Token { .type = TokenType::OPERATOR_DASH             };
   case  '^': return Token { .type = TokenType::OPERATOR_CARET    };
   case  '>': 
      if (peek().has_value() && peek().value() == '=') {
         consume();
         return Token { .type = TokenType::OPERATOR_GREATER_EQUAL };
      }
      return Token { .type = TokenType::OPERATOR_GT               };
   case  '<': 
      if (peek().has_value() && peek().value() == '=' ) {
         consume();
         return Token { .type = TokenType::OPERATOR_LESS_EQUAL    };
      }
      return Token { .type = TokenType::OPERATOR_LT               };
   case  '|':
      if (peek().has_value() && peek().value() == '|') {
         consume();
         return Token { .type = TokenType::OPERATOR_LOGICAL_OR    };
      }
      return Token { .type = TokenType::PIPE                      };
   case '&':
      if (peek().has_value() && peek().value() == '&') {
         consume();
         return Token { .type = TokenType::OPERATOR_LOGICAL_AND   };
      }
      return Token { .type = TokenType::AMPERSAND                 };
   default:  return {};
   }
}

