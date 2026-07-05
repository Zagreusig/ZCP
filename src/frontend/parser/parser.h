#ifndef PARSER_H
#define PARSER_H

#include "Core/Nodes.h"
#include "Core/Tokens.h"
#include <variant>

// enum class UnaryExprType { NEGATIVE };

class Compiler;


class Parser {
public:
   inline explicit Parser(Compiler& cmp, std::vector<Token> tokens)
      : m_tokens(tokens),
        m_ctx(cmp) {}

   std::vector<Token> regurg_toks() { return m_tokens; }

   std::optional<NodeProg> parse_prog();
private:
   [[nodiscard]] inline std::optional<Token> peek(int) const;
   inline Token consume() { return m_tokens.at(m_index++); }
   inline std::optional<Token> try_consume(TokenType);
   inline void consume(int n) { for (int i = 0; i < n; i++) consume(); }
   bool is_next(TokenType, int);
   int get_precidence(BinExprType op);
   // BinExprType bin_type_convert(TokenType);

   // BinExprType comp_to_binop(const TokenType&);
   bool is_compound_assign(const TokenType&);
   static bool is_type(const TokenType&);
   static bool valid_for_increment(const NodeStmt*);
   static bool is_init_stmt(const NodeStmt*);
   static bool is_lval(const NodeExpr*);

   std::optional<TypeInfo>        parse_type();

   std::optional<NodeExpr*>       parse_expr(int);
   std::optional<NodeStmt*>       parse_stmt();
   std::optional<NodeStmt*>       parse_simple_stmt();
   std::optional<NodeCondition*>  parse_cond_primary();
   std::optional<NodeCondition*>  parse_condition_bp(int);
   std::optional<NodeCondition*>  parse_condition();
   std::optional<NodeScopeBlock*> parse_scope();

   std::optional<NodeStmt*>       parse_for();
   std::optional<NodeStmt*>       parse_while();
   std::optional<NodeStmt*>       parse_if();
   std::optional<NodeStmt*>       parse_exit();
   std::optional<NodeStmt*>       parse_return();
   std::optional<NodeStmt*>       parse_have();
   std::optional<NodeStmt*>       parse_print();
   std::optional<NodeStmt*>       finish_assign(NodeExpr*);
   std::optional<NodeStmt*>       wrap_expr_stmt(NodeExpr*);
   std::optional<NodeStmt*>       parse_cmpd_assign();

   std::optional<NodeExpr*>       parse_primary();
   std::optional<NodeExpr*>       parse_ident_expr();
   std::optional<NodeExpr*>       parse_prefix_incdec();
   std::optional<NodeExpr*>       parse_call(Token);

   std::optional<NodeFunction*>   parse_func();
   NodeExpr* wrap(auto*);
   size_t mark() const    { return m_index; }
   void   reset(size_t m) { m_index = m;    }
   
   void sync_next_func();
   void synchronize();
   void fail(const std::string&);

   const std::vector<Token> m_tokens;
   size_t m_index = 0;
   
   Compiler& m_ctx;
};

int cond_precidence(CmpExprType);

#endif // PARSER_H