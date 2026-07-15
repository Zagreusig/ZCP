#include <optional>
#include <vector>
#include <variant>

#include "Core/arena.h"
#include "Core/Nodes.h"
#include "driver/compiler.h"
#include "parser.h"
#include "Core/Tokens.h"
#include "Core/SymbolTable.h"
#include "ErrorHandler.h"
#include "Logger.h"
#include "TokenTable.h"
#include "phase.h"


std::optional<NodeProg> Parser::parse_prog() {
   NodeProg prog;
   try {
      while (peek().has_value()) {
         if (is_next(TokenType::FUNC)){
            if (auto func = parse_func())
               prog.functions.push_back(func.value());
            else
               sync_next_func();
         }
         else {
            int line = 0, col = 0;
            if (peek().has_value()) { line = peek().value().line; col = peek().value().col; }
            m_compiler.diagnostics.error(CompPhase::Parsing, m_compiler.current_filename(), line, col,
                        "Expected function declaration at top level.");
            sync_next_func();
         }
      }
      return prog;
   } catch (const CompilerError&) {
      return prog;
   }
}



[[nodiscard]] inline std::optional<Token> Parser::peek(int offset) const {
   if (m_index + offset >= m_tokens.size()) return {};
   else return m_tokens.at(m_index + offset);
}


bool Parser::is_next(TokenType type, int offset) {
   return peek(offset).has_value() && peek(offset).value().type == type;
}


bool Parser::is_type(const TokenType& t) {
   return t == TokenType::INT || t == TokenType::CHAR ||
          t == TokenType::STR /* || t == TokenType::BOOL || t == TokenType::FLOAT */;
}


bool Parser::is_compound_assign(const TokenType& t) {
   switch (t) {
      case TokenType::OPERATOR_ADD_EQ:
      case TokenType::OPERATOR_SUB_EQ:
      case TokenType::OPERATOR_MUL_EQ:
      case TokenType::OPERATOR_DIV_EQ: return true;
      default:                         return false;
   }
}


bool is_print_stmt(const TokenType& t) {
   switch (t) {
      case TokenType::READC:
      case TokenType::READF:
      case TokenType::READI:
      case TokenType::READS: return true;
      default:               return false;
   }
}


// Disallowing statements for the incremental statements
bool Parser::valid_for_increment(const NodeStmt* s) {
   return !std::holds_alternative<NodeStmtHave*>(s->variant)   &&
          !std::holds_alternative<NodeStmtExit*>(s->variant)   &&
          !std::holds_alternative<NodeStmtReturn*>(s->variant) &&
          !std::holds_alternative<NodeStmtPrint*>(s->variant);
}


bool Parser::is_init_stmt(const NodeStmt* s) {
   return std::holds_alternative<NodeStmtHave*>(s->variant) ||
          std::holds_alternative<NodeStmtAssign*>(s->variant);
}


bool Parser::is_lval(const NodeExpr* x) {
   return std::holds_alternative<NodeExprIdent*>(x->variant) ||
          std::holds_alternative<NodeExprIndex*>(x->variant);
}


std::optional<TypeInfo> Parser::parse_type() {
   TypeInfo info;
   DataType base = Symbols::token_to_datatype(peek().has_value() ? peek().value().type : TokenType::NONE);
   if (base == DataType::NONE) { fail("Expected a type."); return {}; }
   consume(); // type tok
   info.base = base;

   if (try_consume(TokenType::OPEN_BRACKET)) {
      auto len_tok = try_consume(TokenType::INT_LIT);
      if (!len_tok) { fail("Expected array size inside '[ ]'."); return {}; }
      info.is_array = true;
      info.array_len = len_tok.value().int_val();
      if (info.array_len <= 0) { fail("Array size must be positive."); return {}; }
      if (!try_consume(TokenType::CLOSE_BRACKET)) { fail("Expected ']'."); return {}; }
   }
   return info;
}


std::optional<NodeExpr*> Parser::parse_expr(int min_prec = 0) {
   auto left = parse_primary();
   if (!left.has_value()) return {};

   while (peek().has_value()) {
      BinExprType op = Symbols::token_to_binop(peek().value().type);
      if (op == BinExprType::NONE) break;
      int prec = get_precidence(op);
      if (prec < min_prec) break;
      consume();

      int next_prec = (op == BinExprType::EXPONENT ? prec : prec + 1);
      auto right = parse_expr(next_prec);
      if (!right.has_value()) { return {}; }

      NodeBinExpr* bin = m_compiler.allocator.alloc<NodeBinExpr>();
      bin->operation = op; bin->left = left.value(); bin->right = right.value();
      
      left = wrap_expr(bin);
   }
   return left;
}


std::optional<NodeExpr*> Parser::parse_primary() {
   if (is_next(TokenType::OPERATOR_INCR) || is_next(TokenType::OPERATOR_DECR))
      return parse_prefix_incdec();
   
   if (auto val = try_consume(TokenType::INT_LIT)) {
      NodeExprIntLit* i = m_compiler.allocator.alloc<NodeExprIntLit>();
      i->INT_LIT = val.value();
      return wrap_expr(i);
   }

   if (auto val = try_consume(TokenType::CHAR_LIT)) {
      NodeExprCharLit* ch = m_compiler.allocator.alloc<NodeExprCharLit>();
      ch->CHAR_LIT = val.value();
      return wrap_expr(ch);
   }

   if (auto val = try_consume(TokenType::STR_LIT)) {
      NodeExprStrLit* str = m_compiler.allocator.alloc<NodeExprStrLit>();
      str->STR_LIT = val.value();
      return wrap_expr(str);
   }

   if (try_consume(TokenType::OPEN_BRACKET)) {
      NodeExprArrayLit* arr = m_compiler.allocator.alloc<NodeExprArrayLit>();
      if (!is_next(TokenType::CLOSE_BRACKET)) {
         while (true) {
            if (auto e = parse_expr()) arr->elements.push_back(e.value());
            else { fail("Expected expression in array literal."); return {}; }
            if (try_consume(TokenType::COMMA)) continue;
            break;
         }
      }
      if (!try_consume(TokenType::CLOSE_BRACKET)) { fail("Expected ']'."); return {}; }
      return wrap_expr(arr);
   }

   if (peek().has_value() && is_print_stmt(peek().value().type)) {
      NodeExprRead* read = m_compiler.allocator.alloc<NodeExprRead>();
      read->kind = Symbols::token_to_readkind(peek().value().type);
      consume();
      if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {}; }
      if (read->kind == ReadKind::None) { fail("Invalid read type"); return {}; }
      if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {}; }

      return wrap_expr(read);
   }

   if (try_consume(TokenType::OPEN_PAREN)) {
      auto inner = parse_expr(0);
      if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected a matching ')'."); return {}; }
      return inner;
   }
   if (is_next(TokenType::IDENTIFIER))
      return parse_ident_expr();

   return {};
}


std::optional<NodeExpr*> Parser::parse_ident_expr() {
   Token id = consume(); // Identifier

   // Function call
   if (is_next(TokenType::OPEN_PAREN))
      return parse_call(id);

   // array indexing arr[ expr ]
   if (is_next(TokenType::OPEN_BRACKET)) {
      consume(); // [
      auto idx = parse_expr();
      if (!idx.has_value()) { fail("Expected index expression."); return {}; }
      if (!try_consume(TokenType::CLOSE_BRACKET)) { fail("Expected ']'."); return {}; }

      NodeExprIndex* node = m_compiler.allocator.alloc<NodeExprIndex>();
      node->ident = id;
      node->index = idx.value();
      return wrap_expr(node);
   }

   if (is_next(TokenType::OPERATOR_INCR) || is_next(TokenType::OPERATOR_DECR)) {
      bool inc = is_next(TokenType::OPERATOR_INCR);
      consume(); // inc / dec
      NodeExprIncDec* node = m_compiler.allocator.alloc<NodeExprIncDec>();
      node->ident = id; node->is_increment = inc;
      return wrap_expr(node);
   }

   NodeExprIdent* node = m_compiler.allocator.alloc<NodeExprIdent>();
   node->ident = id;
   return wrap_expr(node);
}


std::optional<NodeExpr*> Parser::parse_prefix_incdec() {
   bool inc = is_next(TokenType::OPERATOR_INCR);
   consume(); // ++ / --
   auto id = try_consume(TokenType::IDENTIFIER);
   if (!id.has_value()) { 
      fail(inc ? "Expected variable with '++'." : "Expected variable with '--'."); 
      return {};
   }
   
   NodeExprIncDec* node = m_compiler.allocator.alloc<NodeExprIncDec>();
   node->ident = id.value(); node->is_increment = inc; node->is_prefix = true;
   return wrap_expr(node);
}


std::optional<NodeExpr*> Parser::parse_call(Token name) {
   consume(); // (
   NodeExprCall* call = m_compiler.allocator.alloc<NodeExprCall>();
   call->name = name;
   while (peek().has_value() && peek().value().type != TokenType::CLOSE_PAREN) {
      if (call->args.size() >= 6) {
         fail("Functions currently limited to 6 args.\n");
         return {};
      }

      if (auto arg = parse_expr()) call->args.push_back(arg.value());
      else { return {}; }

      if (is_next(TokenType::COMMA)) consume();
      else break;
   }
   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected closing ')'."); return {}; }
   return wrap_expr(call);
}


std::optional<NodeStmt*> Parser::wrap_expr_stmt(NodeExpr* expr) {
   NodeStmtExpr* stmt_expr = m_compiler.allocator.alloc<NodeStmtExpr>();
   stmt_expr->expr = expr;
   return wrap_stmt(stmt_expr);
}


NodeExpr* Parser::wrap_expr(auto* node) {
   NodeExpr* expr = m_compiler.allocator.alloc<NodeExpr>();
   expr->variant = node;
   return expr;
}


NodeStmt* Parser::wrap_stmt(auto* node) {
   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = node;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_stmt() {
   if (is_next(TokenType::IF))    return parse_if();
   if (is_next(TokenType::WHILE)) return parse_while();
   if (is_next(TokenType::FOR))   return parse_for();
   if (is_next(TokenType::OPEN_BRACE)) {
      auto scope = parse_scope();
      if (!scope) { fail("Error reading scope."); return {}; }
      
      NodeStmtScope* sc = m_compiler.allocator.alloc<NodeStmtScope>();
      sc->scope = scope.value();
      
      NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
      stmt->variant = sc;
      return stmt;
   }
   if (auto stmt = parse_simple_stmt()) {
      if (!try_consume(TokenType::SEMICOLON)) fail("Expected ';'.");
      else return stmt;
   }
   return {};
}


std::optional<NodeStmt*> Parser::parse_simple_stmt() {
   if (is_next(TokenType::_EXIT) && is_next(TokenType::OPEN_PAREN, 1))
      return parse_exit();
   else if (is_next(TokenType::HAVE))
      return parse_have();
   else if (is_next(TokenType::RETURN))
      return parse_return();
   else if ((is_next(TokenType::PRINT) || is_next(TokenType::PRINTLN)) &&
            is_next(TokenType::OPEN_PAREN, 1))
      return parse_print();
   else if (is_next(TokenType::IDENTIFIER)) {
      if (peek(1).has_value() && is_compound_assign(peek(1).value().type))
         return parse_cmpd_assign();

      auto lhs = parse_expr();
      if (!lhs.has_value()) { fail("Expected expression."); return {}; }

      if (try_consume(TokenType::OPERATOR_EQUALS))
         return finish_assign(lhs.value());
      else
         return wrap_expr_stmt(lhs.value());
   }


   if (auto expr = parse_expr()) {
      NodeStmtExpr* stex = m_compiler.allocator.alloc<NodeStmtExpr>();
      stex->expr = expr.value();
      NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
      stmt->variant = stex;
      return stmt;
   }
   return {};
}


std::optional<NodeStmt*> Parser::parse_if() {
   consume(); // if
   if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {}; }

   NodeStmtIf* stmt_if = m_compiler.allocator.alloc<NodeStmtIf>();
   if (auto cond = parse_condition()) stmt_if->condition = cond.value();
   else { fail("Expected if condition.\n"); return {}; }

   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {}; }

   if (auto body = parse_scope()) stmt_if->body = body.value();
   else { fail("Expected if body.\n"); return{}; }

   if (auto val = try_consume(TokenType::ELSE)) {
      if (auto else_body = parse_scope()) stmt_if->else_body = else_body.value();
      else { fail("Expected body in else path.\n"); return {}; }
   }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = stmt_if;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_while() {
   consume(); // while
   if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {}; }

   NodeStmtWhile* _while = m_compiler.allocator.alloc<NodeStmtWhile>();
   if (auto cond = parse_condition()) _while->condition = cond.value();
   else { fail("Expected while condition.\n"); return {}; }

   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'"); return {}; }

   if (auto body = parse_scope()) _while->body = body.value();
   else { fail("Expected while body.\n"); return {}; }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = _while;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_for() {
   consume(); // for
   if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {};}
   NodeStmtFor* _for = m_compiler.allocator.alloc<NodeStmtFor>();

   if (auto init = parse_simple_stmt()) {
      if (!is_init_stmt(init.value())) { fail("Invalid for loop initializer.\n"); return {}; }
      _for->init = init.value();
   }
   else { fail("Expected initializer in for loop.\n"); return {}; }
   if (!try_consume(TokenType::SEMICOLON)) { fail("Expected ';'."); return {}; }

   if (auto cond = parse_condition()) _for->condition = cond.value();
   else { fail("Expected condition in for loop.\n"); return {}; }
   if (!try_consume(TokenType::SEMICOLON)) { fail("Expected ';'."); return {};}

   if (auto inc = parse_simple_stmt()) {
      if (!valid_for_increment(inc.value())) { fail("Invalid for loop incrementer.\n"); return {}; }
      _for->increment = inc.value();
   }
   else { fail("Expected increment in for loop.\n"); return {}; }
   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {};}

   if (auto body = parse_scope()) _for->body = body.value();
   else { fail("Expected for body.\n"); return {}; }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = _for;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_exit() {
   consume(2); // exit(

   NodeStmtExit* stmt_exit = m_compiler.allocator.alloc<NodeStmtExit>();
   if (auto node_expr = parse_expr()) stmt_exit->expr = node_expr.value();
   else {
      fail("Invalid exit expression.");
      return {};
   }
   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {};}

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = stmt_exit;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_have() {
   consume(); // have
   NodeStmtHave* stmt_have = m_compiler.allocator.alloc<NodeStmtHave>();
   auto id = try_consume(TokenType::IDENTIFIER);
   if (!id.has_value()) { fail("Invalid identifier."); return {}; }
   stmt_have->ident = id.value();

   if (try_consume(TokenType::COLON)) {
      if (auto t = parse_type()) {
         stmt_have->has_type = true;
         stmt_have->decl_type = t.value();
      } else {
         return {};
      }
      // array syntax here later
   }

   if (try_consume(TokenType::OPERATOR_EQUALS)) { 
      if (auto expr = parse_expr()) stmt_have->expr = expr.value();
      else { fail("Expected value after '='.\n"); return {}; }
   }
   
   if (!stmt_have->has_type && stmt_have->expr == nullptr) {
      fail("Declaration of '" + stmt_have->ident.text() +
           "' needs type annotation or initializer value.");
      return {};
   }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = stmt_have;
   return stmt;
}


std::optional<NodeStmt*> Parser::finish_assign(NodeExpr* target) {
   if (!is_lval(target)) { fail("Left side of '=' is not assignable."); return {}; }
   auto rhs = parse_expr();
   if (!rhs.has_value()) { fail("Invalid assignment expression."); return {}; }

   NodeStmtAssign* assign = m_compiler.allocator.alloc<NodeStmtAssign>();
   assign->target = target;
   assign->expr = rhs.value();
   
   if (auto* id = std::get_if<NodeExprIdent*>(&target->variant))
      assign->ident = (*id)->ident;
   else if (auto* idx = std::get_if<NodeExprIndex*>(&target->variant))
      assign->ident = (*idx)->ident;

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = assign;
   return stmt;
}


/*
      x += 5;
      x = x + 5;
      ident, assign, binexpr
   
      IDENTIFIER OPERATOR_ADD_EQ INT_LIT SEMICOLON
      
      Assignment -> needs Ident & expr
      wrap ident in NodeExprIdent
      expr -> BinExpr

      BinExpr -> needs operation, left & right NodeExpr
      operation -> set from translated compound type
      lhs -> the identifier Node (for its value), wrapped in NodeExpr
      rhs -> the other value (such as INT_LIT) (which should also be NodeExpr)
   */
std::optional<NodeStmt*> Parser::parse_cmpd_assign() {
   Token ident = consume(); // Identifier
   TokenType optok = consume().type; // Operation + - / *
   BinExprType binop = Symbols::compound_to_binop(optok); // BinExprType translation

   auto rhs = parse_expr();
   if (!rhs.has_value()) { return {}; }

   NodeExprIdent* id_expr = m_compiler.allocator.alloc<NodeExprIdent>();
   id_expr->ident = ident;
   NodeExpr* id_wrapped = m_compiler.allocator.alloc<NodeExpr>();
   id_wrapped->variant = id_expr;

   NodeBinExpr* bin = m_compiler.allocator.alloc<NodeBinExpr>();
   bin->operation = binop;
   bin->left = id_wrapped;
   bin->right = rhs.value();
   NodeExpr* bin_wrap = m_compiler.allocator.alloc<NodeExpr>();
   bin_wrap->variant = bin;

   NodeStmtAssign* assign = m_compiler.allocator.alloc<NodeStmtAssign>();
   assign->target = id_wrapped;
   assign->expr = bin_wrap;

   if (!try_consume(TokenType::SEMICOLON)) { fail("Expected ';'"); return {}; }
   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = assign;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_print() {
   bool with_nl = (peek().value().type == TokenType::PRINTLN);
   consume(2);
   NodeStmtPrint* stmt_print = m_compiler.allocator.alloc<NodeStmtPrint>();
   stmt_print->nwln = with_nl;

   if (auto expr = parse_expr()) stmt_print->expr = expr.value();
   else stmt_print->expr = nullptr;
   
   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {}; }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = stmt_print;
   return stmt;
}


std::optional<NodeStmt*> Parser::parse_return() {
   consume(); // return
   NodeStmtReturn* _return = m_compiler.allocator.alloc<NodeStmtReturn>();
   if (auto expr = parse_expr()) _return->expr = expr.value();
   else { return {}; }

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = _return;
   return stmt;
}



std::optional<NodeScopeBlock*> Parser::parse_scope() {
   if (!try_consume(TokenType::OPEN_BRACE)) { fail("Expected '{'."); return {}; }
   NodeScopeBlock* block = m_compiler.allocator.alloc<NodeScopeBlock>();
   // if (is_next(TokenType::OPEN_BRACE)){
   //    consume();
      while (peek().has_value() && peek().value().type != TokenType::CLOSE_BRACE) {
         if (auto stmt = parse_stmt())
            block->stmts.push_back(stmt.value());
         // Comment parsing
         else if (peek().value().type == TokenType::START_COMMENT_BLOCK) {
            while (peek().has_value() && peek().value().type != TokenType::END_COMMENT_BLOCK)
               consume();
            consume(); // The ender token
         }
         else 
            synchronize(); // Errored and now get back to a spot that's ok.  
      }
      if (!try_consume(TokenType::CLOSE_BRACE)) { fail("Expected '}'."); return {}; }
   // } else {
      // if (auto stmt = parse_stmt())
      //    block->stmts.push_back(stmt.value());
      // else {
      //    fail("Invalid statement." << std::endl;
      //    return {};
      // }
   // }
   
   return block;
}


std::optional<NodeCondition*> Parser::parse_cond_primary() {
   if (peek().has_value() && peek().value().type == TokenType::OPEN_PAREN) {
      size_t saved = mark();
      consume(); // munch (
      if (auto inner = parse_condition_bp(0)) {
         if (peek().has_value() && peek().value().type == TokenType::CLOSE_PAREN) {
            if (peek(1).has_value() &&
                Symbols::token_to_compare(peek(1).value().type) != CmpExprType::NONE)
               reset(saved);
            else {
               consume();
               return inner;
            }
         }
      }
      reset(saved);
   }

   auto left = parse_expr();
   if (!left .has_value()) return {};

   NodeCmpCondition* cmp = m_compiler.allocator.alloc<NodeCmpCondition>();
   cmp->left = left.value();

   CmpExprType op = Symbols::token_to_compare(peek().value().type);
   if (op != CmpExprType::NONE) {
      consume();
      auto right = parse_expr();
      if (!right.has_value()) { fail("Expected comparison operator."); return {}; }
      cmp->operation = op;
      cmp->right = right.value();
   } else {
      cmp->operation = CmpExprType::NONE; // Expr like if (x) or if (!head)
      cmp->right = nullptr;
   }

   NodeCondition* node = m_compiler.allocator.alloc<NodeCondition>();
   node->variant = cmp;
   return node;
}


std::optional<NodeCondition*> Parser::parse_condition_bp(int min_prec) {
   auto left = parse_cond_primary();
   if (!left.has_value()) return {};

   while (peek().has_value()) {
      CmpExprType op = Symbols::token_to_logop(peek().value().type);
      if (op == CmpExprType::NONE) break;

      int prec = cond_precidence(op);
      if (prec < min_prec) break;
      consume();

      auto right = parse_condition_bp(prec + 1);
      if (!right.has_value()) {
         fail("Expected condition after logical operator.");
         return {};
      }

      NodeLogicCondition* logic = m_compiler.allocator.alloc<NodeLogicCondition>();
      logic->operation = op;
      logic->left = left.value();
      logic->right = right.value();

      NodeCondition* node = m_compiler.allocator.alloc<NodeCondition>();
      node->variant = logic;
      left = node;
   }

   return left;
}


std::optional<NodeCondition*> Parser::parse_condition() {
   return parse_condition_bp(0);
}


std::optional<NodeFunction*> Parser::parse_func() {
   if (!peek().has_value() || peek().value().type != TokenType::FUNC) return {};

   NodeFunction* func = m_compiler.allocator.alloc<NodeFunction>();
   consume();  // consume 'func' / 'fn'

   auto name = try_consume(TokenType::IDENTIFIER);
   if (!name.has_value()) { fail("Expected function name."); return {}; }
   if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {}; }
   func->name = name.value();

   if (!is_next(TokenType::CLOSE_PAREN)) {
      while (peek().has_value() && is_type(peek().value().type)) {
         if (func->params.size() >= 6) {
            fail("Functions are capped at 6 params for now.");
            return {};
         }

         NodeParam param;
         if (auto t = parse_type()) param.type = t.value();
         else { fail("Expected parameter type."); return {}; }
         if (auto n = try_consume(TokenType::IDENTIFIER)) param.name = n.value();
         else { fail("Expected parameter name."); return {};}
         func->params.push_back(param);

         if (!try_consume(TokenType::COMMA)) break; 
      }
   }

   if (!try_consume(TokenType::CLOSE_PAREN)) { fail("Expected ')'."); return {}; }

   // Optional return type: either ': type' or '-> type'
   if (try_consume(TokenType::COLON) || try_consume(TokenType::OPERATOR_ARROW)) {
      if (peek().has_value() && is_type(peek().value().type)) {
         func->ret_type = consume();
         func->has_ret_type = true;
      }
      else {
         fail("Expected return type.");
         return {};
      }
   } else func->has_ret_type = false;

   if (auto body = parse_scope())
      func->body = body.value();
   else {
      fail("Expected function body.");
      return {};
   }

   return func;
}


int Parser::get_precidence(BinExprType op) {
   switch (op) {
      case BinExprType::ADDITION:
      case BinExprType::SUBTRACTION:    return 1;
      case BinExprType::MULTIPLICATION:
      case BinExprType::DIVISION:
      case BinExprType::MODULUS:        return 2;
      case BinExprType::EXPONENT:       return 3;
      default:                          return -1;
   }
}


int cond_precidence(CmpExprType op) {
   switch (op) {
      case CmpExprType::OR:  return 1;
      case CmpExprType::AND: return 2;
      default:               return -1; // not a logical op
   }
}


inline std::optional<Token> Parser::try_consume(TokenType type) {
   if (peek().has_value() && peek().value().type == type) return consume();
   else return {};
}


// Something went wrong and now we try to recover parsing to report multiple errors at once.
void Parser::synchronize() {
   m_compiler.log.trace(CompPhase::Parsing, "Synchronizing...", "");
   while (peek().has_value()) {
      if (peek().value().type == TokenType::SEMICOLON) { consume(); return; }
      TokenType t = peek().value().type;
      if (t == TokenType::CLOSE_BRACE || t == TokenType::IF || t == TokenType::WHILE ||
          t == TokenType::FOR || t == TokenType::HAVE || t == TokenType::RETURN ||
          t == TokenType::PRINT || t == TokenType::PRINTLN || t == TokenType::READC ||
          t == TokenType::READF || t == TokenType::READI || t == TokenType::READS) {
         
         return;
      }
      consume();
   }
}


void Parser::fail(const std::string& msg) {
   int line = 0, col = 0;
   if (peek().has_value()) { line = peek().value().line; col = peek().value().col; }
   m_compiler.diagnostics.error(CompPhase::Parsing, m_compiler.current_filename(), line, col, msg);
   synchronize();
 }


void Parser::sync_next_func() {
   while (peek().has_value() && peek().value().type != TokenType::FUNC)
      consume();
}