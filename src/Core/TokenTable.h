#ifndef TOKENTABLE_H
#define TOKENTABLE_H

#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

/*****************************************************************************
* TokenTable.h
* 
* Every token and it's properties live here.
*
* Add new token: add row to TOKEN_DESCRIPTIONS list.
* The TokenType enum, the descriptor table, and every lookup (string name,
* category, precedence, binary-operator mapping) are all generated from that
* list.
*
* Layout of each row:
*  Z(EnumName, "string", Category, precedence, BinExprType)
*
*  - EnumName     : the TokenType enumerator
*  - "string"     : human-readable name (for printing & errors)
*  - Category     : coarse TokenCategory used for parser dispatch
*  - precedence   : binding power for expression parser (0 if N/A)
*  - BinExprType  : the arithmetic op this maps to (BinExprType::NONE if N/A)
*
* Note: cmp/log ops map to CmpExprType, which is queried through a separate 
* helper (cmp_of / logop_of) since a single "binop" column can't hold both
* BinExprType and CmpExprType. Those helpers are defined below and read from
* the same conceptual source, kept adj to the table.
*****************************************************************************/


// ---------------------------------------------------------------------------
// Supporting enums
// ---------------------------------------------------------------------------

enum class BinExprType {
   NONE         = 0,
   ADDITION         = 1,
   SUBTRACTION      = 2,
   MULTIPLICATION   = 3,
   DIVISION         = 4,
   EXPONENT         = 5,
   MODULUS          = 6,
   PARENS           = 7
};

enum class CmpExprType {
   NONE,
   EQUAL,
   AND,
   OR,
   NOT_EQUAL,
   LESS_THAN,
   GREATER_THAN,
   LESS_EQUAL,
   GREATER_EQUAL
};

enum class TokenCategory { 
   KEYWORD,      // Control-flow & decl: if, while, fn, have, return, etc.
   TYPE,         // int, char, str, bool (also kwrds)
   LITERAL,      // int_lit, char_lit, str_lit
   IDENTIFIER,   // user names
   OPERATOR,     // + - * / == != || etc.
   PUNCTUATION,  // ( ) { } [ ] ; , :
   READ,         // readc, readi, readf, reads
   UNIQUE        // EOF, NONE, error toks
};


// ===========================================================================
// THE MASTER LIST
//
// Every token, in enum order. This drives the TokenType enum AND the table.
// Keeping everything generated from this macro makes sure no desync occurs.
//
// Columns: Z(name, "str", category, prec, binop)
// ===========================================================================
#define TOKEN_DESCRIPTORS                                                                                    \
   Z(NONE,                    "none",         TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(_EXIT,                   "exit",         TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(RETURN,                  "return",       TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(PRINT,                   "print",        TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(PRINTLN,                 "println",      TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(READC,                   "readc",        TokenCategory::READ,          0,  BinExprType::NONE)           \
   Z(READS,                   "reads",        TokenCategory::READ,          0,  BinExprType::NONE)           \
   Z(READI,                   "readi",        TokenCategory::READ,          0,  BinExprType::NONE)           \
   Z(READF,                   "readf",        TokenCategory::READ,          0,  BinExprType::NONE)           \
   Z(IF,                      "if",           TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(ELSE,                    "else",         TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(WHEN,                    "when",         TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(FOR,                     "for",          TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(WHILE,                   "while",        TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(BREAK,                   "break",        TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(CONTINUE,                "continue",     TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(FUNC,                    "func",         TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(INT_LIT,                 "int_lit",      TokenCategory::LITERAL,       0,  BinExprType::NONE)           \
   Z(CHAR_LIT,                "char_lit",     TokenCategory::LITERAL,       0,  BinExprType::NONE)           \
   Z(STR_LIT,                 "str_lit",      TokenCategory::LITERAL,       0,  BinExprType::NONE)           \
   Z(INT,                     "int",          TokenCategory::TYPE,          0,  BinExprType::NONE)           \
   Z(CHAR,                    "char",         TokenCategory::TYPE,          0,  BinExprType::NONE)           \
   Z(STR,                     "str",          TokenCategory::TYPE,          0,  BinExprType::NONE)           \
   Z(BOOL,                    "bool",         TokenCategory::TYPE,          0,  BinExprType::NONE)           \
   Z(VOID,                    "void",         TokenCategory::TYPE,          0,  BinExprType::NONE)           \
   Z(TRUE,                    "true",         TokenCategory::LITERAL,       0,  BinExprType::NONE)           \
   Z(FALSE,                   "false",        TokenCategory::LITERAL,       0,  BinExprType::NONE)           \
   Z(UDEF_STRUCT,             "struct",       TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(UDEF_CLASS,              "class",        TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(PUBLIC,                  "public",       TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(PRIVATE,                 "private",      TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(IDENTIFIER,              "identifier",   TokenCategory::IDENTIFIER,    0,  BinExprType::NONE)           \
   Z(HAVE,                    "have",         TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(COLON,                   ":",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(SEMICOLON,               ";",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(DOUBLE_QUOTE,            "\"",           TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(APOSTRAPHE,              "'",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(FULL_STOP,               ".",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(COMMA,                   ",",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(QUESTION,                "?",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(AMPERSAND,               "&",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(PIPE,                    "|",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(UNDERSCORE,              "_",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(NEGATIVE_SIGN,           "-",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_BANG,           "!",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPEN_PAREN,              "(",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(OPEN_BRACE,              "{",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(OPEN_BRACKET,            "[",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(CLOSE_PAREN,             ")",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(CLOSE_BRACE,             "}",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(CLOSE_BRACKET,           "]",            TokenCategory::PUNCTUATION,   0,  BinExprType::NONE)           \
   Z(OPERATOR_EQUALS,         "=",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_PLUS,           "+",            TokenCategory::OPERATOR,      1,  BinExprType::ADDITION)       \
   Z(OPERATOR_ASTERISK,       "*",            TokenCategory::OPERATOR,      2,  BinExprType::MULTIPLICATION) \
   Z(OPERATOR_SLASH,          "/",            TokenCategory::OPERATOR,      2,  BinExprType::DIVISION)       \
   Z(OPERATOR_DASH,           "-",            TokenCategory::OPERATOR,      1,  BinExprType::SUBTRACTION)    \
   Z(OPERATOR_CARET,          "^",            TokenCategory::OPERATOR,      3,  BinExprType::EXPONENT)       \
   Z(OPERATOR_PERCENT,        "%",            TokenCategory::OPERATOR,      2,  BinExprType::MODULUS)        \
   Z(OPERATOR_INCR,           "++",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_DECR,           "--",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_ADD_EQ,         "+=",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_SUB_EQ,         "-=",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_MUL_EQ,         "*=",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_DIV_EQ,         "/=",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(OPERATOR_LT,             "<",            TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_GT,             ">",            TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_EQUAL_EQUAL,    "==",           TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_NOT_EQUAL,      "!=",           TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_LESS_EQUAL,     "<=",           TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_GREATER_EQUAL,  ">=",           TokenCategory::OPERATOR,      5,  BinExprType::NONE)           \
   Z(OPERATOR_LOGICAL_AND,    "&&",           TokenCategory::OPERATOR,      4,  BinExprType::NONE)           \
   Z(OPERATOR_LOGICAL_OR,     "||",           TokenCategory::OPERATOR,      3,  BinExprType::NONE)           \
   Z(OPERATOR_ARROW,          "->",           TokenCategory::OPERATOR,      0,  BinExprType::NONE)           \
   Z(ESCAPE_CHAR,             "escape",       TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(COMMENT,                 "comment",      TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(START_COMMENT_BLOCK,     "/*",           TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(END_COMMENT_BLOCK,       "*/",           TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(INVALID,                 "invalid",      TokenCategory::UNIQUE,        0,  BinExprType::NONE)           \
   Z(CONST,                   "const",        TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(GLOBAL,                  "global",       TokenCategory::KEYWORD,       0,  BinExprType::NONE)           \
   Z(POUND,                   "#",            TokenCategory::OPERATOR,      0,  BinExprType::NONE)


// ---------------------------------------------------------------------------
// Generating the TokenType enum from the master list
// ---------------------------------------------------------------------------
enum class TokenType {
#define Z(name, str, cat, prec, binop) name,
   TOKEN_DESCRIPTORS
#undef Z
   COUNT    // Senti value, num of token types & table size
};


// ---------------------------------------------------------------------------
// The descriptor struct + table
// ---------------------------------------------------------------------------
struct TokenDesc {
   TokenType        type;
   std::string_view str;
   TokenCategory    category;
   int              precedence;
   BinExprType      binop;
};


namespace detail {
   inline constexpr std::array<TokenDesc, static_cast<size_t>(TokenType::COUNT)>
      TOKEN_TABLE = {{
#define Z(name, str, cat, prec, binop) TokenDesc{ TokenType::name, str, cat, prec, binop },
        TOKEN_DESCRIPTORS
#undef Z
      }};

   // If a row is missing or extra, this fails at compile time
   static_assert(TOKEN_TABLE.size() == static_cast<size_t>(TokenType::COUNT),
                 "TOKEN_TABLE size must match number of TokenType values.");
}


// ---------------------------------------------------------------------------
// Lookups - all read from the single table, indexed directly by enum val.
// (The enum is contiguous because it's generated from the same list, so
//  static_cast<size_t>(type) is a valid index in [0, COUNT). )
// ---------------------------------------------------------------------------
inline constexpr const TokenDesc& descriptor_of(TokenType t) {
   return detail::TOKEN_TABLE[static_cast<size_t>(t)];
}

inline constexpr std::string_view to_string(TokenType t) {
   return descriptor_of(t).str;
}

inline constexpr TokenCategory category_of(TokenType t) {
   return descriptor_of(t).category;
}

inline constexpr int precedence_of(TokenType t) {
   return descriptor_of(t).precedence;
}

inline constexpr BinExprType binop_of(TokenType t) {
   return descriptor_of(t).binop;
}


// ---------------------------------------------------------------------------
// Categorys!
// Add a token to the right category in the table and these pick it up
// without the need to edit a corresponding lookup switch statement.
// ---------------------------------------------------------------------------
inline constexpr bool is_type(TokenType t)     { return category_of(t) == TokenCategory::TYPE; }
inline constexpr bool is_literal(TokenType t)  { return category_of(t) == TokenCategory::LITERAL; }
inline constexpr bool is_operator(TokenType t) { return category_of(t) == TokenCategory::OPERATOR; }
inline constexpr bool is_keyword(TokenType t)  { return category_of(t) == TokenCategory::KEYWORD; }
inline constexpr bool is_read(TokenType t)     { return category_of(t) == TokenCategory::READ; }
inline constexpr bool is_punct(TokenType t)    { return category_of(t) == TokenCategory::PUNCTUATION; }

// is this a binary arithmetic operator ?
inline constexpr bool is_binop(TokenType t)    { return binop_of(t) != BinExprType::NONE; }


// ---------------------------------------------------------------------------
// Compatison / log op mappings.
// These can't live within the "binop" column, so they're here as focused 
// switches. Not an issue, as there are only a small handful of tokens here.
// ---------------------------------------------------------------------------
inline constexpr CmpExprType cmp_of(TokenType t) {
   switch (t) {
      case TokenType::OPERATOR_EQUAL_EQUAL:   return CmpExprType::EQUAL;
      case TokenType::OPERATOR_NOT_EQUAL:     return CmpExprType::NOT_EQUAL;
      case TokenType::OPERATOR_GREATER_EQUAL: return CmpExprType::GREATER_EQUAL;
      case TokenType::OPERATOR_GT:            return CmpExprType::GREATER_THAN;
      case TokenType::OPERATOR_LESS_EQUAL:    return CmpExprType::LESS_EQUAL;
      case TokenType::OPERATOR_LT:            return CmpExprType::LESS_THAN;
      default:                                return CmpExprType::NONE;
   }
}

inline constexpr CmpExprType logop_of(TokenType t) {
   switch (t) {
      case TokenType::OPERATOR_LOGICAL_AND: return CmpExprType::AND;
      case TokenType::OPERATOR_LOGICAL_OR:  return CmpExprType::OR;
      default:                              return CmpExprType::NONE;
   }
}

inline constexpr bool is_cmp(TokenType t)   { return cmp_of(t)   != CmpExprType::NONE; }
inline constexpr bool is_logop(TokenType t) { return logop_of(t) != CmpExprType::NONE; }


// ---------------------------------------------------------------------------
// Compound assignmnet mappings -> the arithmetic op is maps to.
// ---------------------------------------------------------------------------
inline constexpr BinExprType compound_binop_of(TokenType t) {
   switch (t) {
      case TokenType::OPERATOR_ADD_EQ: return BinExprType::ADDITION;
      case TokenType::OPERATOR_SUB_EQ: return BinExprType::SUBTRACTION;
      case TokenType::OPERATOR_MUL_EQ: return BinExprType::MULTIPLICATION;
      case TokenType::OPERATOR_DIV_EQ: return BinExprType::DIVISION;
      default:                         return BinExprType::NONE;
   }
}

inline constexpr bool is_compound_assign(TokenType t) {
   return compound_binop_of(t) != BinExprType::NONE;
}
#endif // TOKENTABLE_H