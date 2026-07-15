#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <variant>
#include <optional>
#include <string>
#include <vector>

#include "Core/Nodes.h"
#include "Core/Tokens.h"

enum class BinExprType;
enum class CmpExprType;
enum class TokenType;

// enum class UnaryExprType { NEGATIVE };

class Compiler;


class Parser {
public:
   inline explicit Parser(Compiler& cmp, std::vector<Token> tokens)
      : m_tokens(tokens),
        m_compiler(cmp) {}

   std::vector<Token> regurg_toks() { return m_tokens; }

   std::optional<NodeProg> parse_prog();
private:
   [[nodiscard]] inline std::optional<Token> peek(int offset = 0) const;
   inline Token consume() { return m_tokens.at(m_index++); }
   inline std::optional<Token> try_consume(TokenType type);
   inline void consume(int n) { for (int i = 0; i < n; i++) consume(); }
   bool is_next(TokenType type, int offset = 0);
   int get_precidence(BinExprType op);
   // BinExprType bin_type_convert(TokenType);

   // BinExprType comp_to_binop(const TokenType&);
   bool is_compound_assign(const TokenType& token);
   static bool is_type(const TokenType& token);
   static bool valid_for_increment(const NodeStmt* statement);
   static bool is_init_stmt(const NodeStmt* statement);
   static bool is_lval(const NodeExpr* expression);

   std::optional<TypeInfo>        parse_type();

   std::optional<NodeExpr*>       parse_expr(int precedence);
   std::optional<NodeStmt*>       parse_stmt();
   std::optional<NodeStmt*>       parse_simple_stmt();
   std::optional<NodeCondition*>  parse_cond_primary();
   std::optional<NodeCondition*>  parse_condition_bp(int precedence);
   std::optional<NodeCondition*>  parse_condition();
   std::optional<NodeScopeBlock*> parse_scope();

   std::optional<NodeStmt*>       parse_for();
   std::optional<NodeStmt*>       parse_while();
   std::optional<NodeStmt*>       parse_if();
   std::optional<NodeStmt*>       parse_exit();
   std::optional<NodeStmt*>       parse_return();
   std::optional<NodeStmt*>       parse_have();
   std::optional<NodeStmt*>       parse_print();
   std::optional<NodeStmt*>       finish_assign(NodeExpr* expression);
   std::optional<NodeStmt*>       wrap_expr_stmt(NodeExpr* expression);
   std::optional<NodeStmt*>       parse_cmpd_assign();

   std::optional<NodeExpr*>       parse_primary();
   std::optional<NodeExpr*>       parse_ident_expr();
   std::optional<NodeExpr*>       parse_prefix_incdec();
   std::optional<NodeExpr*>       parse_call(Token token);

   std::optional<NodeFunction*>   parse_func();
   NodeExpr* wrap_expr(auto* expression);
   NodeStmt* wrap_stmt(auto* statement);
   size_t mark() const    { return m_index; }
   void   reset(size_t m) { m_index = m;    }
   
   void sync_next_func();
   void synchronize();
   void fail(const std::string& msg);

   const std::vector<Token> m_tokens;
   size_t m_index = 0;
   
   Compiler& m_compiler;
};

int cond_precidence(CmpExprType type);

#endif // PARSER_H