#ifndef NODES_H
#define NODES_H

#include <variant>
#include <vector>
#include "Tokens.h"

enum class DataType { NONE, INT, CHAR, STR, BOOL, FLOAT };

struct TypeInfo {
   DataType base = DataType::NONE;

   bool is_ptr   = false;

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
      if (base == DataType::STR) return 16; // fat pointer: ptr(8) + len(8)
      return is_array ? array_len * elem_size() : elem_size();
   }
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

struct NodeExprArrayLit {
   std::vector<NodeExpr*> elements;
};

struct NodeExprIndex {
   Token ident;
   NodeExpr* index = nullptr;
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
   std::variant<NodeExprIntLit*, NodeExprCharLit*, NodeExprStrLit*,
                NodeExprIdent*, NodeExprIndex*, NodeExprRead*,
                NodeExprIncDec*, NodeBinExpr*, NodeExprCall*, NodeExprArrayLit*> var;

   TypeInfo resolved;
   bool is_resolved = false;
};



// -------------- STATEMENTS -------------------------



struct NodeStmtExit {
   NodeExpr* expr = nullptr;
};

struct NodeStmtHave {
   Token ident;
   bool has_type = false;
   TypeInfo decl_type;       // valid when has_type
   NodeExpr* expr = nullptr; // init, nullptr if not.

   TypeInfo resolved;        // Stashed type
   bool is_resolved;         // Computed yet?
};

struct NodeStmtAssign {
   Token ident;
   NodeExpr* target = nullptr; // lvalue !!!
   NodeExpr* expr   = nullptr;
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
   TypeInfo type;
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
   std::variant<NodeStmtExit*, NodeStmtExpr*, NodeStmtHave*, NodeScopeBlock*, NodeStmtIf*, 
                NodeStmtWhile*, NodeStmtAssign*, NodeStmtFor*, NodeStmtReturn*,  
                NodeStmtScope*, NodeStmtPrint*> var;
};

struct NodeProg {
   std::vector<NodeFunction*> funcs;
};

#endif // NODES_H