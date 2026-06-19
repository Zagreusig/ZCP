#ifndef NODES_H
#define NODES_H

#include <variant>
#include <vector>
#include "lexer/Tokens.h"

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

struct NodeExpr {
   std::variant<NodeExprIntLit*, NodeExprIdent*, NodeBinExpr*, NodeExprCall*, 
                NodeExprCharLit*, NodeExprStrLit*, NodeExprIncDec*> var;
};

struct NodeStmtExit {
   NodeExpr* expr = nullptr;
};

struct NodeStmtHave {
   Token ident;
   NodeExpr* expr = nullptr;
};

struct NodeStmtAssign {
   Token ident;
   NodeExpr* expr = nullptr;
};

struct NodeScopeBlock {
   std::vector<NodeStmt*> stmts;
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
                NodeStmtPrint*, NodeStmtExpr*> var;
};

struct NodeProg {
   std::vector<NodeFunction*> funcs;
};

#endif // NODES_H