#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include "Nodes.h"
#include "IRDefs.h"
#include <string>

namespace Symbols {

   inline bool is_compd_assign(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_ADD_EQ:
         case TokenType::OPERATOR_SUB_EQ:
         case TokenType::OPERATOR_MUL_EQ:
         case TokenType::OPERATOR_DIV_EQ: return true;
         default:                         return false;
      }
   }

   inline bool is_read_stmt(TokenType t) {
      switch (t) {
         case TokenType::READC:
         case TokenType::READF:
         case TokenType::READS:
         case TokenType::READI: return true;
         default:               return false;
      }
   }

   inline DataType token_to_datatype(TokenType t) {
      switch (t) {
         case TokenType::INT:
         case TokenType::INT_LIT:  return DataType::INT;
         case TokenType::CHAR:
         case TokenType::CHAR_LIT: return DataType::CHAR;
         case TokenType::STR:
         case TokenType::STR_LIT:  return DataType::STR;
         case TokenType::BOOL:     
         case TokenType::FALSE:
         case TokenType::TRUE:     return DataType::BOOL;
         default:                  return DataType::NONE;
      }
   }

   inline TokenType datatype_to_token(DataType t) {
      switch (t) {
         case DataType::INT:  return TokenType::INT;
         case DataType::CHAR: return TokenType::CHAR;
         case DataType::STR:  return TokenType::STR;
         case DataType::BOOL: return TokenType::BOOL;
         default:             return TokenType::NONE;
      }
   }

   inline std::string datatype_to_str(DataType t) {
      switch (t) {
         case DataType::INT:  return "INT";
         case DataType::CHAR: return "CHAR";
         case DataType::STR:  return "STR";
         case DataType::BOOL: return "BOOL";
         default:             return "NULL";
      }
   }

   inline ReadKind token_to_readkind(TokenType t) {
      switch (t) {
         case TokenType::READC: return ReadKind::Char;
         case TokenType::READI: return ReadKind::Int;
         case TokenType::READF: return ReadKind::None;
         case TokenType::READS: return ReadKind::Line;
         default:               return ReadKind::None;
      }
   }

   inline BinExprType compound_to_binop(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_ADD_EQ: return BinExprType::ADDITION;
         case TokenType::OPERATOR_SUB_EQ: return BinExprType::SUBTRACTION;
         case TokenType::OPERATOR_MUL_EQ: return BinExprType::MULTIPLICATION;
         case TokenType::OPERATOR_DIV_EQ: return BinExprType::DIVISION;
         default:                         return BinExprType::NONE;
      }
   }

   inline BinExprType token_to_binop(TokenType t) {
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

   inline CmpExprType token_to_compare(TokenType t) {
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

   inline CmpExprType token_to_logop(TokenType t) {
      switch (t) {
         case TokenType::OPERATOR_LOGICAL_AND: return CmpExprType::AND;
         case TokenType::OPERATOR_LOGICAL_OR:  return CmpExprType::OR;
         default:                              return CmpExprType::NONE;
      }
   }

   inline IROp binop_to_ir(BinExprType op) {
      switch (op) {
         case BinExprType::ADDITION:       return IROp::Add;
         case BinExprType::SUBTRACTION:    return IROp::Sub;
         case BinExprType::MULTIPLICATION: return IROp::Mul;
         case BinExprType::DIVISION:       return IROp::Div;
         case BinExprType::MODULUS:        return IROp::Mod;
         default:                          return IROp::Add;
      }
   }

   inline IROp cmp_to_ir(CmpExprType op) {
      switch (op) {
         case CmpExprType::EQUAL:         return IROp::CmpEq;
         case CmpExprType::NOT_EQUAL:     return IROp::CmpNe;
         case CmpExprType::LESS_THAN:     return IROp::CmpLt;
         case CmpExprType::LESS_EQUAL:    return IROp::CmpLe;
         case CmpExprType::GREATER_THAN:  return IROp::CmpGt;
         case CmpExprType::GREATER_EQUAL: return IROp::CmpGe;
         default:                         return IROp::CmpEq; // && / || handled specifically
      }
   }

   
   inline IRType datatype_to_irtype(DataType type) {
      switch (type) {
         case DataType::INT: return IRType::I64;
         case DataType::CHAR:
         case DataType::BOOL: return IRType::I8;
         case DataType::STR:  return IRType::Ptr; // fat pointer handled later
         default:             return IRType::I64;
      }
   }

   inline IRType ir_type_of(const TypeInfo& t) {
      if (t.is_ptr || t.is_array) return IRType::Ptr;
      return datatype_to_irtype(t.base);
   }

   inline IRType ir_type_of(TokenType type) {
      return datatype_to_irtype(token_to_datatype(type));
   }

   
};

#endif // SYMBOLTABLE_H