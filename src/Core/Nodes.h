#ifndef NODES_H
#define NODES_H

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "Tokens.h"

enum class DataType { NONE, INT, CHAR, STR, BOOL, FLOAT, STRUCT, CLASS };

struct StructLayout; // frwrd decl

struct TypeInfo {
   DataType base = DataType::NONE;
   bool is_signed = true;
   bool is_ptr   = false;
   bool is_array = false;
   int array_len = 0;

   const StructLayout* struct_layout = nullptr;
   std::string unresolved_name = "";

   // Element in bytes.
   int element_size() const;

   // total storage this var occupies on the stack.
   int byte_size() const;
};

struct TypedName { Token name; std::optional<TypeInfo> type; };

struct NodeExpr;
struct NodeStmt;
struct NodeCondition;

struct NodeExprIntLit   { Token INT_LIT; };
struct NodeExprCharLit  { Token CHAR_LIT; };
struct NodeExprStrLit   { Token STR_LIT; };
struct NodeExprBoolLit  { Token BOOL_LIT; };
struct NodeExprIdent    { Token ident; };
struct NodeExprArrayLit { std::vector<NodeExpr*> elements; };

struct NodeExprIndex {
   Token ident;
   NodeExpr* index = nullptr;
};

struct NodeCmpCondition {
   ComparisonOp operation;
   NodeExpr* left  = nullptr;
   NodeExpr* right = nullptr;
};

struct NodeLogicCondition {
   LogicOp operation;
   NodeCondition* left  = nullptr;
   NodeCondition* right = nullptr;
};

struct NodeCondition {
   std::variant<NodeCmpCondition*, NodeLogicCondition*> variant;
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
   DataType kind = DataType::CHAR; // Defaulting to char (1 byte read)
};

struct NodeExprUnary {
   UnaryExprType op;
   NodeExpr* operand = nullptr;
};

struct NodeExprField {
   NodeExpr* base = nullptr;
   Token field;
};

struct NodeExprNew { Token type_name; };

struct NodeExpr {
   std::variant<NodeExprIntLit*, NodeExprCharLit*, NodeExprStrLit*, NodeExprBoolLit*,
                NodeExprIdent*, NodeExprIndex*, NodeExprRead*,
                NodeExprIncDec*, NodeBinExpr*, NodeExprCall*, NodeExprArrayLit*,
                NodeExprUnary*, NodeExprField*, NodeExprNew*> variant;

   TypeInfo resolved;
   bool is_resolved = false;
};



// -------------- STATEMENTS -------------------------



struct NodeStmtExit { NodeExpr* expr = nullptr; };

struct NodeStmtHave {
   TypedName decl;
   NodeExpr* expr = nullptr; // init, nullptr if not.

   TypeInfo resolved;        // Stashed type
   bool is_resolved = false; // Computed yet?
};

struct NodeStmtAssign {
   Token ident;
   NodeExpr* target = nullptr; // lvalue !!!
   NodeExpr* expr   = nullptr;
};

struct NodeScopeBlock { std::vector<NodeStmt*> stmts; };
struct NodeStmtScope { NodeScopeBlock* scope = nullptr; };

struct NodeStmtIf {
   NodeCondition*  condition = nullptr;
   NodeScopeBlock* body      = nullptr;
   NodeScopeBlock* else_body = nullptr;
};

struct NodeStmtWhile {
   NodeCondition*  condition = nullptr;
   NodeScopeBlock* body      = nullptr;
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

struct NodeStmtReturn { NodeExpr* expr = nullptr; };

struct NodeStmtPrint {
   NodeExpr* expr = nullptr;
   bool nwln      = false;
};

struct NodeStmtExpr { NodeExpr* expr = nullptr;  };

struct NodeStmt {
   std::variant<NodeStmtExit*, NodeStmtExpr*, NodeStmtHave*, NodeScopeBlock*, NodeStmtIf*, 
                NodeStmtWhile*, NodeStmtAssign*, NodeStmtFor*, NodeStmtReturn*,  
                NodeStmtScope*, NodeStmtPrint*> variant;
};


// -------------- struct / class nodes ----------------
struct NodeStructField {
   TypedName decl;
   int offset = 0;
   /** FUTURE: default, visibility */
};


struct NodeStructDecl {
   Token name;
   std::vector<NodeStructField> vars = {};
};

// ------ TOP LEVEL NODES ---------------


struct NodeFunction {
   Token ret_type;
   bool has_ret_type = false;
   Token name;
   std::vector<NodeParam> params;
   NodeScopeBlock* body = nullptr;
};


struct NodeTypeDecl {
   std::variant<NodeStructDecl*> variant; // NodeEnumDecl*, etc. WIP!!!!!
};


struct NodeTopLevel {
   std::variant<NodeFunction*, NodeTypeDecl*> variant;
};

struct NodeProg {
   std::vector<NodeFunction*> functions;
   std::vector<NodeTopLevel*> declarations;
};

#endif // NODES_H