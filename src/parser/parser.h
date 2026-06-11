#ifndef PARSER_H
#define PARSER_H

#include "ast/arena.h"
#include "lexer/lexer.h"
#include <variant>

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

// enum class UnaryExprType { NEGATIVE };

struct NodeExpr;
struct NodeStmt;
struct NodeCondition;

struct NodeExprIntLit {
   Token INT_LIT;
};

struct NodeExprCharLit {
   Token CHAR_LIT;
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

struct NodeExprCall {
   Token name;
   std::vector<NodeExpr*> args;
};

struct NodeExpr {
   std::variant<NodeExprIntLit*, NodeExprIdent*, NodeBinExpr*, NodeExprCall*, NodeExprCharLit*> var;
};

struct NodeStmtChar {
   Token ident;
   NodeExpr* expr = nullptr;
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

struct NodeStmt {
   std::variant<NodeStmtExit*, NodeStmtHave*, NodeScopeBlock*, NodeStmtIf*, 
                NodeStmtWhile*, NodeStmtFor*, NodeStmtAssign*, NodeStmtReturn*> var;
};

struct NodeProg {
   std::vector<NodeFunction*> funcs;
};

class Parser {
public:
   inline explicit Parser(std::vector<Token> tokens)
      : m_tokens(std::move(tokens)),
        m_allocator(1024 * 1024 * 4) {}

   std::vector<Token> regurg_toks() { return m_tokens; }

   std::optional<NodeExpr*>       parse_expr(int);
   std::optional<NodeStmt*>       parse_stmt();
   std::optional<NodeCondition*>  parse_cond_primary();
   std::optional<NodeCondition*>  parse_condition_bp(int);
   std::optional<NodeCondition*>  parse_condition();
   std::optional<NodeScopeBlock*> parse_scope();
   std::optional<NodeFunction*>   parse_func();
   std::optional<NodeProg>        parse_prog();
private:
   [[nodiscard]] inline std::optional<Token> peek(int) const;
   inline Token consume() { return m_tokens.at(m_index++); }
   
   inline Token try_consume(TokenType, const std::string&);
   inline std::optional<Token> try_consume(TokenType);
   inline void consume(int n) { for (int i = 0; i < n; i++) consume(); }
   int get_precidence(BinExprType op);
   BinExprType bin_type_convert(TokenType);

   size_t mark() const    { return m_index; }
   void   reset(size_t m) { m_index = m;    }

   const std::vector<Token> m_tokens;
   size_t m_index = 0;
   ArenaAllocator m_allocator;
};

int cond_precidence(CmpExprType);

#endif // PARSER_H