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
enum class ComparisonOp;
enum class LogicOp;
enum class UnaryExprType;
enum class TokenType;
struct TypedName;
class Compiler;

class Parser {
public:
   inline explicit Parser(Compiler& cmp, std::vector<Token> tokens, std::string file_name)
      : m_tokens(tokens), m_file_name(file_name), m_compiler(cmp) {}

   std::vector<Token> regurg_toks() { return m_tokens; }

   std::optional<NodeProg> parse_prog();
private:
   [[nodiscard]] inline std::optional<Token> peek(int offset = 0) const;
   inline Token consume() { return m_tokens.at(m_index++); }
   inline std::optional<Token> try_consume(TokenType type);
   inline void consume(int n) { for (int i = 0; i < n; i++) consume(); }
   bool is_next(TokenType type, int offset = 0);
   int get_precidence(BinExprType op);

   bool is_compound_assign(const TokenType& token);
   static bool is_type(const TokenType& token);
   static bool valid_for_increment(const NodeStmt* statement);
   static bool is_init_stmt(const NodeStmt* statement);
   static bool is_lval(const NodeExpr* expression);

   std::optional<TypeInfo>        parse_type();
   std::optional<TypedName>       parse_typed_name();

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


   std::optional<NodeExpr*>       parse_unary();
   std::optional<NodeExpr*>       parse_primary();
   std::optional<NodeExpr*>       parse_ident_expr();
   std::optional<NodeExpr*>       parse_prefix_incdec();
   std::optional<NodeExpr*>       parse_call(Token token);
   std::optional<NodeExpr*>       parse_member_access(Token token);
   
   std::optional<NodeFunction*>   parse_func();
   std::optional<NodeTypeDecl*>   parse_type_decl();
   std::optional<NodeStructDecl*> parse_struct_decl();
   NodeExpr* wrap_expr(auto* expression);
   NodeStmt* wrap_stmt(auto* statement);
   NodeTopLevel* wrap_top(auto* decl);
   NodeTypeDecl* wrap_type(auto* type);
   size_t mark() const       { return m_index; }
   void   reset(size_t mark) { m_index = mark; }
   
   void sync_next_top_level();
   void synchronize();
   void fail(const std::string& msg);

   const std::vector<Token> m_tokens;
   size_t m_index = 0;
   
   std::string m_file_name;
   Compiler& m_compiler;
};

int cond_precidence(LogicOp type);

#endif // PARSER_H