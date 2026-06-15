#ifndef PARSER_H
#define PARSER_H

#include "ast/arena.h"
#include "ast/Nodes.h"
#include "lexer/Tokens.h"
#include <variant>

// enum class UnaryExprType { NEGATIVE };


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