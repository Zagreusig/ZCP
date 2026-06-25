#ifndef NODES_H
#define NODES_H

#include <variant>
#include <vector>
#include "lexer/Tokens.h"

enum class DataType { NONE, INT, CHAR, STR, BOOL };

struct TypeInfo {
   DataType base = DataType::NONE;
   bool is_array = false;
   int array_len = 0;

   // Element in bytes.
   int elem_size() const {
      switch (base) {
         case DataType::CHAR: return 1;
         case DataType::INT:  return 8;
         default:             return 8;
      }
   }

   // total storage this var occupies on the stack.
   int byte_size() const {
      return is_array ? array_len * elem_size() : elem_size();
   }
};

enum BinExprType {
   NONE             = 0,
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

enum class ReadKind { None, Char, Int, Float, Line }; // Line = String

struct NodeExpr;
struct NodeStmt;
struct NodeCondition;

struct NodeExprIntLit {
   Token INT_LIT;
};

struct NodeExprCharLit {
   Token CHAR_LIT;
};

struct NodeExprStrLit {
   Token STR_LIT;
};

struct NodeExprIdent {
   Token ident;
};

struct NodeCmpCondition {
   CmpExprType operation;
   NodeExpr* left  = nullptr;
   NodeExpr* right = nullptr;
};

struct NodeLogicCondition {
   CmpExprType operation;
   NodeCondition* left  = nullptr;
   NodeCondition* right = nullptr;
};

struct NodeCondition {
   std::variant<NodeCmpCondition*, NodeLogicCondition*> var;
};

struct NodeBinExpr {
   BinExprType operation;
   NodeExpr* left  = nullptr;
   NodeExpr* right = nullptr;
};

struct NodeExprIncDec {
   Token ident;
   bool is_increment = false;
   bool is_prefix    = false;
};

struct NodeExprCall {
   Token name;
   std::vector<NodeExpr*> args;
};

struct NodeExprRead {
   ReadKind kind = ReadKind::Char; // Defaulting to char (1 byte read)
};

struct NodeExpr {
   std::variant<NodeExprIntLit*, NodeExprIdent*, NodeBinExpr*, NodeExprCall*, 
                NodeExprCharLit*, NodeExprStrLit*, NodeExprIncDec*, NodeExprRead*> var;
};

struct NodeStmtExit {
   NodeExpr* expr = nullptr;
};

struct NodeStmtHave {
   Token ident;
   bool has_type = false;
   TypeInfo decl_type;       // valid when has_type
   NodeExpr* expr = nullptr; // init, nullptr if not.
};

struct NodeStmtAssign {
   Token ident;
   NodeExpr* expr = nullptr;
};

struct NodeScopeBlock {
   std::vector<NodeStmt*> stmts;
};

struct NodeStmtScope {
   NodeScopeBlock* scope = nullptr;
};

struct NodeStmtIf {
   NodeCondition* condition  = nullptr;
   NodeScopeBlock* body      = nullptr;
   NodeScopeBlock* else_body = nullptr;
};

struct NodeStmtWhile {
   NodeCondition* condition = nullptr;
   NodeScopeBlock* body     = nullptr;
};

struct NodeStmtFor {
   NodeStmt* init           = nullptr;
   NodeStmt* increment      = nullptr;
   NodeCondition* condition = nullptr;
   NodeScopeBlock* body     = nullptr;
};


struct NodeParam {
   Token type;
   Token name;
};

struct NodeStmtReturn {
   NodeExpr* expr = nullptr;
};

struct NodeFunction {
   Token ret_type;
   bool has_ret_type = false;
   Token name;
   std::vector<NodeParam> params;
   NodeScopeBlock* body = nullptr;
};

struct NodeStmtPrint {
   NodeExpr* expr = nullptr;
   bool nwln      = false;
};

struct NodeStmtExpr {
   NodeExpr* expr = nullptr; 
};

struct NodeStmt {
   std::variant<NodeStmtExit*, NodeStmtHave*, NodeScopeBlock*, NodeStmtIf*, 
                NodeStmtWhile*, NodeStmtFor*, NodeStmtAssign*, NodeStmtReturn*,  
                NodeStmtPrint*, NodeStmtExpr*, NodeStmtScope*> var;
};

struct NodeProg {
   std::vector<NodeFunction*> funcs;
};

#endif // NODES_H