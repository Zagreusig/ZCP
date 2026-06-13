#include <iostream>
#include <optional>
#include <vector>
#include "ast/arena.h"
#include "parser.h"
#include "lexer/lexer.h"

[[nodiscard]] inline std::optional<Token> Parser::peek(int offset = 0) const {
   if (m_index + offset >= m_tokens.size()) return {};
   else return m_tokens.at(m_index + offset);
}


BinExprType Parser::bin_type_convert(TokenType type) {
   switch (type) {
   case TokenType::OPERATOR_CARET:    return BinExprType::EXPONENT;
   case TokenType::OPERATOR_ASTERISK: return BinExprType::MULTIPLICATION;
   case TokenType::OPERATOR_SLASH:    return BinExprType::DIVISION;
   case TokenType::OPERATOR_PERCENT:  return BinExprType::MODULUS;
   case TokenType::OPERATOR_PLUS:     return BinExprType::ADDITION;
   case TokenType::OPERATOR_DASH:     return BinExprType::SUBTRACTION;
   default:                           return BinExprType::NONE;
   }
}

CmpExprType cmp_type_convert(TokenType type) {
   switch (type) {
      case TokenType::OPERATOR_EQUAL_EQUAL:   return CmpExprType::EQUAL;
      case TokenType::OPERATOR_NOT_EQUAL:     return CmpExprType::NOT_EQUAL;
      case TokenType::OPERATOR_GREATER_EQUAL: return CmpExprType::GREATER_EQUAL;
      case TokenType::OPERATOR_GT:            return CmpExprType::GREATER_THAN;
      case TokenType::OPERATOR_LESS_EQUAL:    return CmpExprType::LESS_EQUAL;
      case TokenType::OPERATOR_LT:            return CmpExprType::LESS_THAN;
      default:                                return CmpExprType::NONE;
   }
}


static CmpExprType logic_op_convert(TokenType t) {
   switch(t) {
      case TokenType::OPERATOR_LOGICAL_OR:  return CmpExprType::OR;
      case TokenType::OPERATOR_LOGICAL_AND: return CmpExprType::AND;
      default:                              return CmpExprType::NONE;
   }
}


std::optional<NodeExpr*> Parser::parse_expr(int min_prec = 0) {
   std::optional<NodeExpr*> left;

   if (auto val = try_consume(TokenType::INT_LIT)) {
      NodeExprIntLit* int_lit = m_allocator.alloc<NodeExprIntLit>();
      int_lit->INT_LIT = val.value();
      NodeExpr* expr = m_allocator.alloc<NodeExpr>();
      expr->var = int_lit;
      left = expr;
   }
   else if (auto val = try_consume(TokenType::OPEN_PAREN)) {
      left = parse_expr(0);
      if (!peek().has_value() || peek().value().type != TokenType::CLOSE_PAREN) {
         std::cerr << "Expected ')'" << std::endl;
         exit(EXIT_FAILURE);
      }
      consume();
   }
   else if (auto val = try_consume(TokenType::IDENTIFIER)){
      if (peek().has_value() && peek().value().type == TokenType::OPEN_PAREN) {
         NodeExprCall* call = m_allocator.alloc<NodeExprCall>();
         call->name = val.value();
         consume();

         while (peek().has_value() && peek().value().type != TokenType::CLOSE_PAREN) {
            if (call->args.size() >= 6) {
               std::cerr << "Functions are currently capped at 6 arguments." << std::endl;
               exit(EXIT_FAILURE);
            }

            if (auto expr = parse_expr())
               call->args.push_back(expr.value());
            else {
               std::cerr << "Expected expression in argument list." << std::endl;
               exit(EXIT_FAILURE);
            }

            if (peek().has_value() && peek().value().type == TokenType::COMMA)
               consume();
            else break;
         }
         try_consume(TokenType::CLOSE_PAREN, "Expected ')' after arguments.");

         NodeExpr* expr = m_allocator.alloc<NodeExpr>();
         expr->var = call;
         left = expr;
      } else {
         NodeExprIdent* identifier = m_allocator.alloc<NodeExprIdent>();
         identifier->ident = val.value();
         NodeExpr* expr = m_allocator.alloc<NodeExpr>();
         expr->var = identifier;
         left = expr;
      }
   }
   else if (auto val = try_consume(TokenType::CHAR_LIT)) {
      std::cout << "Parsed char lit.\n";
      NodeExprCharLit* char_lit = m_allocator.alloc<NodeExprCharLit>();
      char_lit->CHAR_LIT = val.value();
      NodeExpr* expr = m_allocator.alloc<NodeExpr>();
      expr->var = char_lit;
      left = expr;
   }
   else return {};

   while (peek().has_value()) {
      BinExprType op = bin_type_convert(peek().value().type);
      if (op == BinExprType::NONE) break;

      int prec = get_precidence(op);
      if (prec < min_prec) break;

      consume();

      int next_prec = (op == BinExprType::EXPONENT) ? prec : prec + 1;
      auto right = parse_expr(next_prec);
      if (!right.has_value()) {
         std::cerr << "Expected math expression." << std::endl;
         exit(EXIT_FAILURE);
      }

      NodeBinExpr* bin = m_allocator.alloc<NodeBinExpr>();
      bin->operation = op;
      bin->left = left.value();
      bin->right = right.value();

      NodeExpr* expr = m_allocator.alloc<NodeExpr>();
      expr->var = bin;
      left = expr;
   }

   return left;
}


std::optional<NodeStmt*> Parser::parse_stmt() {
   if (peek().value().type == TokenType::_EXIT && peek(1).has_value() &&
         peek(1).value().type == TokenType::OPEN_PAREN) {
      consume(2);
      NodeStmtExit* stmt_exit = m_allocator.alloc<NodeStmtExit>();
      if (auto node_expr = parse_expr())
         stmt_exit->expr = node_expr.value();
      else {
         std::cerr << "Invalid exit expression." << std::endl;
         exit(EXIT_FAILURE);
      }
      if (peek().has_value() && peek().value().type == TokenType::CLOSE_PAREN) consume();
      else {
         std::cerr << "Expected a ')'." << std::endl;
         exit(EXIT_FAILURE); 
      }
      try_consume(TokenType::SEMICOLON, "Expected a ';'");

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_exit;
      return stmt;
   }
   else if (peek().has_value() && peek().value().type == TokenType::HAVE &&
            peek(1).has_value() && peek(1).value().type == TokenType::IDENTIFIER &&
            peek(2).has_value() && peek(2).value().type == TokenType::OPERATOR_EQUALS) {
      consume();
      NodeStmtHave* stmt_have = m_allocator.alloc<NodeStmtHave>();
      stmt_have->ident = consume();
      consume();
      if (auto expr = parse_expr()) stmt_have->expr = expr.value();
      else {
         std::cerr << "Invalid declaration" << std::endl;
         exit(EXIT_FAILURE);
      }
      try_consume(TokenType::SEMICOLON, "Expected ';'");

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_have;
      return stmt;
   }
   else if (auto val = try_consume(TokenType::IF)) {
      try_consume(TokenType::OPEN_PAREN, "Expected a '(' after 'if'.");

      NodeStmtIf* stmt_if = m_allocator.alloc<NodeStmtIf>();
      if (auto cond = parse_condition())
         stmt_if->condition = cond.value();
      else {
         std::cerr << "Expected if condition." << std::endl;
         exit(EXIT_FAILURE);
      }

      try_consume(TokenType::CLOSE_PAREN, "Expected ')' after condition.");

      if (!peek().has_value() || peek().value().type != TokenType::OPEN_BRACE) {
         std::cerr << "Expected '{' after if condition" << std::endl;
         exit(EXIT_FAILURE);
      }
      if (auto body = parse_scope())
         stmt_if->body = body.value();

      if (auto val = try_consume(TokenType::ELSE)) {
         if (!peek().has_value() || peek().value().type != TokenType::OPEN_BRACE) {
            std::cerr << "Expected '{' after else." << std::endl;
            exit(EXIT_FAILURE);
         }
         if (auto else_body = parse_scope())
            stmt_if->else_body = else_body.value();
      }

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = stmt_if;
      return stmt;
   }
   else if (auto val = try_consume(TokenType::WHILE)) {
      try_consume(TokenType::OPEN_PAREN, "Expected '(' after while.");

      NodeStmtWhile* _while = m_allocator.alloc<NodeStmtWhile>();
      if (auto cond = parse_condition())
         _while->condition = cond.value();
      else {
         std::cerr << "Expected while condition." << std::endl;
         exit(EXIT_FAILURE);
      }

      try_consume(TokenType::CLOSE_PAREN, "Expected ')' after condition.");

      if (auto body = parse_scope())
         _while->body = body.value();
      else {
         std::cerr << "Expected while body" << std::endl;
         exit(EXIT_FAILURE);
      }

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = _while;
      return stmt;
   }
   else if (auto val = try_consume(TokenType::FOR)) {
      try_consume(TokenType::OPEN_PAREN, "Expected '(' after for.");

      NodeStmtFor* _for = m_allocator.alloc<NodeStmtFor>();

      if (auto init = parse_stmt())
         _for->init = init.value();
      else {
         std::cerr << "Expected initializer in for loop." << std::endl;
         exit(EXIT_FAILURE);
      }

      if (auto cond = parse_condition())
         _for->condition = cond.value();
      else {
         std::cerr << "Expected a condition in for loop." << std::endl;
         exit(EXIT_FAILURE);
      }
      try_consume(TokenType::SEMICOLON, "Expected ';' after condition.");

      NodeStmtAssign* assign = m_allocator.alloc<NodeStmtAssign>();
      assign->ident = try_consume(TokenType::IDENTIFIER, "Expected identifier in for loop.");
      try_consume(TokenType::OPERATOR_EQUALS, "Expected '=' in for loop");

      if (auto expr = parse_expr())
         assign->expr = expr.value();
      else {
         std::cerr << "Expected expression in for loop" << std::endl;
         exit(EXIT_FAILURE);
      }

      _for->increment = m_allocator.alloc<NodeStmt>();
      _for->increment->var = assign;
      try_consume(TokenType::CLOSE_PAREN, "Expected ')' at end of for loop.");

      if (auto body = parse_scope())
         _for->body = body.value();
      else {
         std::cerr << "Expected body in for loop." << std::endl;
         exit(EXIT_FAILURE);
      }

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = _for;
      return stmt;
   }
   else if (peek().has_value() && peek().value().type == TokenType::IDENTIFIER &&
            peek(1).has_value() && peek(1).value().type == TokenType::OPERATOR_EQUALS) {
      NodeStmtAssign* assign = m_allocator.alloc<NodeStmtAssign>();
      assign->ident = consume();
      consume();

      if (auto expr = parse_expr())
         assign->expr = expr.value();
      else {
         std::cerr << "Invalid assignment expression." << std::endl;
         exit(EXIT_FAILURE);
      }

      try_consume(TokenType::SEMICOLON, "Expected ';'");

      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = assign;
      return stmt;
   }
   else if (auto val = try_consume(TokenType::RETURN)) {
      NodeStmtReturn* _return = m_allocator.alloc<NodeStmtReturn>();
      if (auto expr = parse_expr())
         _return->expr = expr.value();
      else {
         std::cerr << "Expected expression after return." << std::endl;
         exit(EXIT_FAILURE);
      }

      try_consume(TokenType::SEMICOLON, "Expected ';' after return.");
      NodeStmt* stmt = m_allocator.alloc<NodeStmt>();
      stmt->var = _return;
      return stmt;
   }
   
   else return {};
}


std::optional<NodeScopeBlock*> Parser::parse_scope() {
   try_consume(TokenType::OPEN_BRACE, "Expected '{'.");
   NodeScopeBlock* block = m_allocator.alloc<NodeScopeBlock>();
   while (peek().has_value() && peek().value().type != TokenType::CLOSE_BRACE) {
      if (auto stmt = parse_stmt())
         block->stmts.push_back(stmt.value());
      else {
         std::cerr << "Invalid statement in block." << std::endl;
         exit(EXIT_FAILURE);
      }
   }

   try_consume(TokenType::CLOSE_BRACE, "Expected '}'");
   return block;
}


std::optional<NodeCondition*> Parser::parse_cond_primary() {
   if (peek().has_value() && peek().value().type == TokenType::OPEN_PAREN) {
      size_t saved = mark();
      consume(); // munch (
      if (auto inner = parse_condition_bp(0)) {
         if (peek().has_value() && peek().value().type == TokenType::CLOSE_PAREN) {
            if (peek(1).has_value() &&
                cmp_type_convert(peek(1).value().type) != CmpExprType::NONE)
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

   NodeCmpCondition* cmp = m_allocator.alloc<NodeCmpCondition>();
   cmp->left = left.value();

   CmpExprType op = cmp_type_convert(peek().value().type);
   if (op != CmpExprType::NONE) {
      consume();
      auto right = parse_expr();
      if (!right.has_value()) {
         std::cerr << "Expected comparison operator." << std::endl;
         exit(EXIT_FAILURE);
      }
      cmp->operation = op;
      cmp->right = right.value();
   } else {
      cmp->operation = CmpExprType::NONE; // Expr like if (x) or if (!head)
      cmp->right = nullptr;
   }

   NodeCondition* node = m_allocator.alloc<NodeCondition>();
   node->var = cmp;
   return node;
}


std::optional<NodeCondition*> Parser::parse_condition_bp(int min_prec) {
   auto left = parse_cond_primary();
   if (!left.has_value()) return {};

   while (peek().has_value()) {
      CmpExprType op = logic_op_convert(peek().value().type);
      if (op == CmpExprType::NONE) break;

      int prec = cond_precidence(op);
      if (prec < min_prec) break;
      consume();

      auto right = parse_condition_bp(prec + 1);
      if (!right.has_value()) {
         std::cerr << "Expected condition after logical operator." << std::endl;
         exit(EXIT_FAILURE);
      }

      NodeLogicCondition* logic = m_allocator.alloc<NodeLogicCondition>();
      logic->operation = op;
      logic->left = left.value();
      logic->right = right.value();

      NodeCondition* node = m_allocator.alloc<NodeCondition>();
      node->var = logic;
      left = node;
   }

   return left;
}


std::optional<NodeCondition*> Parser::parse_condition() {
   return parse_condition_bp(0);
}


std::optional<NodeFunction*> Parser::parse_func() {
   if (!peek().has_value() || peek().value().type != TokenType::FUNC) return {};

   NodeFunction* func = m_allocator.alloc<NodeFunction>();
   consume();  // consume 'func' / 'fn' — it's a keyword, not the return type

   func->name = try_consume(TokenType::IDENTIFIER, "Expected function name.");
   try_consume(TokenType::OPEN_PAREN, "Expected '(' after function name.");

   while (peek().has_value() && peek().value().type != TokenType::CLOSE_PAREN) {
      if (func->params.size() >= 6) {
         std::cerr << "Functions are capped at 6 params for now." << std::endl;
         exit(EXIT_FAILURE);
      }

      NodeParam param;
      param.type = try_consume(TokenType::INT, "Expected parameter type.");
      param.name = try_consume(TokenType::IDENTIFIER, "Expected parameter name.");
      func->params.push_back(param);

      if (peek().has_value() && peek().value().type == TokenType::COMMA)
         consume();
      else break;
   }

   try_consume(TokenType::CLOSE_PAREN, "Expected ')' after '('.");

   // Optional return type: either ': type' or '-> type'
   if (try_consume(TokenType::COLON) || try_consume(TokenType::OPERATOR_ARROW)) {
      func->ret_type = try_consume(TokenType::INT, "Expected return type after ':' or '->'.");
      func->has_ret_type = true;
   } else {
      func->has_ret_type = false;
   }

   if (auto body = parse_scope())
      func->body = body.value();
   else {
      std::cerr << "Expected function body." << std::endl;
      exit(EXIT_FAILURE);
   }

   return func;
}




std::optional<NodeProg> Parser::parse_prog() {
   NodeProg prog;
   while (peek().has_value()) {
      if (auto func = parse_func())
         prog.funcs.push_back(func.value());
      else {
         std::cerr << "Invalid top-level declaration." << std::endl;
         exit(EXIT_FAILURE);
      }
   }
   return prog;
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


inline Token Parser::try_consume(TokenType type, const std::string& errMsg) {
   if (peek().has_value() && peek().value().type == type) return consume();
   else {
      std::cerr << errMsg << std::endl;
      exit(EXIT_FAILURE);
   }
}