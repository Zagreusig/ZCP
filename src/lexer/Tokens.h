#ifndef TOKENS_H
#define TOKENS_H

#include <optional>
#include <string>

#define TOKEN_TYPES               \
   X(NONE)                        \
   X(_EXIT)                       \
   X(RETURN)                      \
   X(PRINT)                       \
   X(PRINTLN)                     \
   X(IF)                          \
   X(ELSE)                        \
   X(WHEN)                        \
   X(FOR)                         \
   X(WHILE)                       \
   X(BREAK)                       \
   X(CONTINUE)                    \
   X(FUNC)                        \
   X(INT_LIT)                     \
   X(CHAR_LIT)                    \
   X(STR_LIT)                     \
   X(INT)                         \
   X(CHAR)                        \
   X(STR)                         \
   X(BOOL)                        \
   X(IDENTIFIER)                  \
   X(HAVE)                        \
   X(COLON)                       \
   X(SEMICOLON)                   \
   X(DOUBLE_QUOTE)                \
   X(APOSTRAPHE)                  \
   X(FULL_STOP)                   \
   X(COMMA)                       \
   X(QUESTION)                    \
   X(AMPERSAND)                   \
   X(PIPE)                        \
   X(UNDERSCORE)                  \
   X(NEGATIVE_SIGN)               \
   X(OPERATOR_BANG)               \
   X(OPEN_PAREN)                  \
   X(OPEN_BRACE)                  \
   X(OPEN_BRACKET)                \
   X(CLOSE_PAREN)                 \
   X(CLOSE_BRACE)                 \
   X(CLOSE_BRACKET)               \
   X(OPERATOR_EQUALS)             \
   X(OPERATOR_PLUS)               \
   X(OPERATOR_ASTERISK)           \
   X(OPERATOR_SLASH)              \
   X(OPERATOR_DASH)               \
   X(OPERATOR_CARET)              \
   X(OPERATOR_PERCENT)            \
   X(OPERATOR_INCR)               \
   X(OPERATOR_DECR)               \
   X(OPERATOR_ADD_EQ)             \
   X(OPERATOR_SUB_EQ)             \
   X(OPERATOR_MUL_EQ)             \
   X(OPERATOR_DIV_EQ)             \
   X(OPERATOR_LT)                 \
   X(OPERATOR_GT)                 \
   X(OPERATOR_EQUAL_EQUAL)        \
   X(OPERATOR_NOT_EQUAL)          \
   X(OPERATOR_LESS_EQUAL)         \
   X(OPERATOR_GREATER_EQUAL)      \
   X(OPERATOR_LOGICAL_AND)        \
   X(OPERATOR_LOGICAL_OR)         \
   X(OPERATOR_ARROW)              \
   X(ESCAPE_CHAR)                 \
   X(COMMENT)                     \
   X(START_COMMENT_BLOCK)         \
   X(END_COMMENT_BLOCK)


enum class TokenType {
#define X(name) name,
    TOKEN_TYPES
#undef X
};

inline const char* to_string(const TokenType t) {
    switch (t) {
#define X(name) case TokenType::name: return #name;
        TOKEN_TYPES
#undef X
    }
    return "UNKNOWN";
}

struct Token {
   TokenType type                   = TokenType::NONE;
   std::optional<std::string> value = "[null]";
};

#endif // TOKENS_H