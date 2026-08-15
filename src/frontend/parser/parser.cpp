#include <optional>
#include <vector>
#include <variant>

#include "Core/arena.h"
#include "Core/Nodes.h"
#include "driver/compiler.h"
#include "debug/Log.h"
#include "parser.h"
#include "Core/Tokens.h"
#include "Core/TypeConversions.h"
#include "TokenTable.h"
#include "phase.h"

std::optional<NodeProg> Parser::parse_prog() {
   NodeProg prog;
   prog.declarations.reserve(100);
   try {
      while (peek().has_value()) {
         NodeTopLevel* decl = m_compiler.allocator.alloc<NodeTopLevel>();
         if (is_next(TokenType::FUNC)){
            if (auto func = parse_func()) {
               decl->variant = func.value();
               prog.declarations.push_back(decl);
            }
            else
               sync_next_top_level();
         }
         else if (is_next(TokenType::UDEF_STRUCT) || is_next(TokenType::UDEF_CLASS)) {
            if (auto type = parse_type_decl()) {
               decl->variant = type.value();
               prog.declarations.push_back(decl);
            }
         }
         // else if (is_next(ENUMS)), else if (is_next(GLOBAL))
         else {
            if (peek().has_value()) { 
               Log::error(CompPhase::Parsing, "Expected function declaration at top level.");
               sync_next_top_level();
            }
         }
      }
      return prog;
   } catch (const CompilerError&) {
      return prog;
   }
}


std::optional<NodeTypeDecl*> Parser::parse_type_decl() {
   NodeTypeDecl* decl = m_compiler.allocator.alloc<NodeTypeDecl>();
   if (is_next(TokenType::UDEF_STRUCT)) {
      if (auto _struct = parse_struct_decl())
         decl->variant = _struct.value();
   }
   else return {};
   
   return decl;
}


/** WIP:
 *  This way doesn't allow for default values yet.
 */
std::optional<NodeStructDecl*> Parser::parse_struct_decl() {
   if (!peek().has_value() || !is_next(TokenType::UDEF_STRUCT)) return {};
   NodeStructDecl* def = m_compiler.allocator.alloc<NodeStructDecl>();
   consume(); // UDEF_STRUCT
   
   if (!is_next(TokenType::IDENTIFIER)) { fail("Expected struct identifier."); return {}; }
   Token name = consume();
   def->name = name;

   if (!is_next(TokenType::OPEN_BRACE) && is_next(TokenType::SEMICOLON)) 
      return def; // foreward declaration.
   consume();
   /** TODO: this */
   
   int struct_size = 0;
   while (peek().has_value() && peek().value().type != TokenType::CLOSE_BRACE) {
      if (auto type = parse_typed_name(); type.has_value()) {
         NodeStructField field;

         if (!type.value().type.has_value()) { fail("Expected type notation for struct field."); return {}; }
         def->vars.push_back(NodeStructField{ .decl = type.value() , .offset = struct_size});
         struct_size += type.value().type.value().byte_size();
         if (!try_consume(TokenType::SEMICOLON)) { fail("Expected ';' after field declaration."); return {}; }
      }
   }
   if (!try_consume(TokenType::CLOSE_BRACE)) { fail("Expected closing '}' in struct declaration"); return {}; }
   if (!try_consume(TokenType::SEMICOLON))   { fail("Expected ';' after struct declaration."); return {}; }

   return def;
}


[[nodiscard]] inline std::optional<Token> Parser::peek(int offset) const {
   if (m_index + offset >= m_tokens.size()) return {};
   else return m_tokens.at(m_index + offset);
}


bool Parser::is_next(TokenType type, int offset) {
   return peek(offset).has_value() && peek(offset).value().type == type;
}


bool Parser::is_primitive_type(const TokenType& t) {
   return t == TokenType::INT || t == TokenType::CHAR ||
          t == TokenType::STR || t == TokenType::BOOL /* || t == TokenType::FLOAT */;
}


bool Parser::is_type(const TokenType& t) {
   return is_primitive_type(t); /** TEMP: */
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


bool is_read_stmt(const TokenType& t) {
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
          std::holds_alternative<NodeExprIndex*>(x->variant) ||
          std::holds_alternative<NodeExprField*>(x->variant);
}


std::optional<TypeInfo> Parser::parse_type() {
   TypeInfo info;
   DataType base = Symbols::token_to_datatype(peek().has_value() ? peek().value().type : TokenType::NONE);
   if (peek().has_value() && peek().value().type == TokenType::IDENTIFIER) { 
      base = DataType::STRUCT;
      info.unresolved_name = peek().value().text();
   }
   else if (base == DataType::NONE) { fail("Expected a type."); return {}; }
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


std::optional<TypedName> Parser::parse_typed_name() {
   auto id = try_consume(TokenType::IDENTIFIER);
   if (!id.has_value()) { fail("Expected identifier."); return {}; }
   Token name = id.value();
   if (!try_consume(TokenType::COLON)) return TypedName{ .name = name, .type = std::nullopt };

   TypeInfo info;
   if (auto type = parse_type(); type.has_value()) info = type.value();
   else return TypedName{ .name = name, .type = std::nullopt };

   return TypedName{ .name = name, .type = info };
}


std::optional<NodeExpr*> Parser::parse_expr(int min_prec = 0) {
   auto left = parse_unary();
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


std::optional<NodeExpr*> Parser::parse_unary() {
   UnaryExprType op = peek().has_value() ? Symbols::token_to_unop(peek().value().type) : UnaryExprType::NONE;
   if (op == UnaryExprType::NONE) return parse_primary();
   consume();

   auto operand = parse_unary(); // right-assoc: !!x, -(-x) via nesting
   if (!operand.has_value()) { fail("Expected expression after unary operator."); return {}; }

   NodeExprUnary* u = m_compiler.allocator.alloc<NodeExprUnary>();
   u->op = op;
   u->operand = operand.value();
   return wrap_expr(u);
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

   if (auto val = try_consume(TokenType::TRUE)) {
      NodeExprBoolLit* _bool = m_compiler.allocator.alloc<NodeExprBoolLit>();
      _bool->BOOL_LIT = val.value();
      return wrap_expr(_bool);
   }

   if (auto val = try_consume(TokenType::FALSE)) {
      NodeExprBoolLit* _bool = m_compiler.allocator.alloc<NodeExprBoolLit>();
      _bool->BOOL_LIT = val.value();
      return wrap_expr(_bool);
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

   if (peek().has_value() && is_read_stmt(peek().value().type)) {
      NodeExprRead* read = m_compiler.allocator.alloc<NodeExprRead>();
      read->kind = Symbols::token_to_readkind(peek().value().type);
      consume();
      if (!try_consume(TokenType::OPEN_PAREN)) { fail("Expected '('."); return {}; }
      if (read->kind == DataType::NONE) { fail("Invalid read type"); return {}; }
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

   // ++ / --
   if (is_next(TokenType::OPERATOR_INCR) || is_next(TokenType::OPERATOR_DECR)) {
      bool inc = is_next(TokenType::OPERATOR_INCR);
      consume(); // inc / dec
      NodeExprIncDec* node = m_compiler.allocator.alloc<NodeExprIncDec>();
      node->ident = id; node->is_increment = inc;
      return wrap_expr(node);
   }

   if (is_next(TokenType::FULL_STOP)) { // ident.member ( eventually ident->member )
      consume(); // .
      return parse_member_access(id);
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

   auto decl = parse_typed_name();
   if (!decl.has_value()) return {}; // parse_typed_name()/parse_type() already reported the error.
   stmt_have->decl = decl.value();
   // array syntax here later

   if (try_consume(TokenType::OPERATOR_EQUALS)) {
      if (auto expr = parse_expr()) stmt_have->expr = expr.value();
      else { fail("Expected value after '='.\n"); return {}; }
   }

   if (!stmt_have->decl.type.has_value() && stmt_have->expr == nullptr) {
      fail("Declaration of '" + stmt_have->decl.name.text() +
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
   else if (auto* fld = std::get_if<NodeExprField*>(&target->variant))
      assign->ident = (*fld)->field;

   NodeStmt* stmt = m_compiler.allocator.alloc<NodeStmt>();
   stmt->variant = assign;
   return stmt;
}


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
   assign->ident = ident;
   assign->target = id_wrapped;
   assign->expr = bin_wrap;

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
                Symbols::token_to_compare(peek(1).value().type) != ComparisonOp::NONE)
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

   ComparisonOp op = Symbols::token_to_compare(peek().value().type);
   if (op != ComparisonOp::NONE) {
      consume();
      auto right = parse_expr();
      if (!right.has_value()) { fail("Expected comparison operator."); return {}; }
      cmp->operation = op;
      cmp->right = right.value();
   } else {
      cmp->operation = ComparisonOp::NONE; // Expr like if (x) or if (!head)
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
      LogicOp op = Symbols::token_to_logop(peek().value().type);
      if (op == LogicOp::NONE) break;

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
         else { param.name.value = "Null"; }
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

   if (try_consume(TokenType::SEMICOLON)) {
      func->body = nullptr;
   } else if (auto body = parse_scope()) {
      func->body = body.value();
   }
   else {
      fail("Expected function body or ';'.");
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


int cond_precidence(LogicOp op) {
   switch (op) {
      case LogicOp::OR:  return 1;
      case LogicOp::AND: return 2;
      default:            return -1; // not a logical op
   }
}


inline std::optional<Token> Parser::try_consume(TokenType type) {
   if (peek().has_value() && peek().value().type == type) return consume();
   else return {};
}


// Something went wrong and now we try to recover parsing to report multiple errors at once.
void Parser::synchronize() {
   Log::trace(CompPhase::Parsing, "Encountered error, syncing...");
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
   Log::error(CompPhase::Parsing, msg, m_file_name, line, col);
   synchronize();
}


void Parser::sync_next_top_level() {
   Log::trace(CompPhase::Parsing, "Encountered error, syncing to next function...");
   while (peek().has_value() && 
          (peek().value().type != TokenType::FUNC 
          || peek().value().type != TokenType::UDEF_STRUCT))
      consume();
}


NodeTopLevel* Parser::wrap_top(auto* node) {
   NodeTopLevel* top = m_compiler.allocator.alloc<NodeTopLevel>();
   top->variant = node;
   return top;
}


NodeTypeDecl* Parser::wrap_type(auto* node) {
   NodeTypeDecl* type = m_compiler.allocator.alloc<NodeTypeDecl>();
   type->variant = node;
   return type;
}

// how do we know what 'x' 's type is ?
// foo.x = 5;
std::optional<NodeExpr*> Parser::parse_member_access(Token identifier) {
   NodeExprField* field = m_compiler.allocator.alloc<NodeExprField>();
   if (is_next(TokenType::FULL_STOP)) consume(); // just in case
   if (is_next(TokenType::IDENTIFIER)) field->field = consume();
   
   NodeExprIdent* ident = m_compiler.allocator.alloc<NodeExprIdent>();
   ident->ident = identifier; field->base = wrap_expr(ident);
   
   return wrap_expr(field);
}