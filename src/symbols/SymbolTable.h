#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include "ast/Nodes.h"
#include "codegen/type_checker.h"
#include "lexer/Tokens.h"

namespace Symbols {

   static bool is_compd_assign(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_ADD_EQ:
         case TokenType::OPERATOR_SUB_EQ:
         case TokenType::OPERATOR_MUL_EQ:
         case TokenType::OPERATOR_DIV_EQ: return true;
         default:                         return false;
      }
   }

   static bool is_read_stmt(TokenType t) {
      switch (t) {
         case TokenType::READC:
         case TokenType::READF:
         case TokenType::READS:
         case TokenType::READI: return true;
         default:               return false;
      }
   }

   static DataType tok_dt(TokenType t) {
      switch (t) {
         case TokenType::INT:
         case TokenType::INT_LIT:  return DataType::INT;  break;
         case TokenType::CHAR:
         case TokenType::CHAR_LIT: return DataType::CHAR; break;
         case TokenType::STR:
         case TokenType::STR_LIT:  return DataType::STR;  break;
         default:                  return DataType::NONE;
      }
   }

   static ReadKind tok_rk(TokenType t) {
      switch (t) {
         case TokenType::READC: return ReadKind::Char;
         case TokenType::READI: return ReadKind::Int;
         case TokenType::READF: return ReadKind::None;
         case TokenType::READS: return ReadKind::Line;
         default:               return ReadKind::None;
      }
   }

   static BinExprType cmpd_binop(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_ADD_EQ: return BinExprType::ADDITION;
         case TokenType::OPERATOR_SUB_EQ: return BinExprType::SUBTRACTION;
         case TokenType::OPERATOR_MUL_EQ: return BinExprType::MULTIPLICATION;
         case TokenType::OPERATOR_DIV_EQ: return BinExprType::DIVISION;
         default:                         return BinExprType::NONE;
      }
   }

   static BinExprType tok_binop(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_CARET:    return BinExprType::EXPONENT;
         case TokenType::OPERATOR_ASTERISK: return BinExprType::MULTIPLICATION;
         case TokenType::OPERATOR_SLASH:    return BinExprType::DIVISION;
         case TokenType::OPERATOR_PERCENT:  return BinExprType::MODULUS;
         case TokenType::OPERATOR_PLUS:     return BinExprType::ADDITION;
         case TokenType::OPERATOR_DASH:     return BinExprType::SUBTRACTION;
         default:                           return BinExprType::NONE;
      }
   }

   static CmpExprType tok_cmp(TokenType t) {
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

   static CmpExprType tok_logop(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_LOGICAL_AND: return CmpExprType::AND;
         case TokenType::OPERATOR_LOGICAL_OR:  return CmpExprType::OR;
         default:                              return CmpExprType::NONE;
      }
   }
};

#endif // SYMBOLTABLE_H