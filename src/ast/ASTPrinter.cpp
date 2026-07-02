#include "ASTPrinter.h"
#include "symbols/SymbolTable.h"

std::string ASTPrinter::cmp_name(CmpExprType op) {
   switch (op) {
      case CmpExprType::EQUAL:         return "EQUAL";
      case CmpExprType::NOT_EQUAL:     return "NOT_EQUAL";
      case CmpExprType::LESS_THAN:     return "LESS_THAN";
      case CmpExprType::GREATER_THAN:  return "GREATER_THAN";
      case CmpExprType::LESS_EQUAL:    return "LESS_EQUAL";
      case CmpExprType::GREATER_EQUAL: return "GREATER_EQUAL";
      case CmpExprType::AND:           return "AND";
      case CmpExprType::OR:            return "OR";
      case CmpExprType::NONE:          return "NONE";
      default:                         return "?";
   }
}

std::string ASTPrinter::bin_name(BinExprType op) {
   switch (op) {
      case BinExprType::ADDITION:       return "ADD";
      case BinExprType::SUBTRACTION:    return "SUB";
      case BinExprType::MULTIPLICATION: return "MUL";
      case BinExprType::DIVISION:       return "DIV";
      case BinExprType::MODULUS:        return "MOD";
      case BinExprType::EXPONENT:       return "EXP";
      default:                          return "?";
   }
}

void ASTPrinter::print_function(const NodeFunction* func, int depth) {
   if (func == nullptr) { std::cout << pad(depth) << "<null func>\n"; return; }
   std::cout << pad(depth) << "Function: " << func->name.value.value();
   if (func->has_ret_type)
      std::cout << " -> " << to_string(func->ret_type.type);
   std::cout << "\n";

   if (!func->params.empty()) {
      std::cout << pad(depth + 1) << "Params:\n";
      for (const NodeParam& p : func->params)
         std::cout << pad(depth + 2)
                   << Symbols::dt_str(p.type.base) << " "
                   << p.name.value.value() << "\n";
   }

   std::cout << pad(depth + 1) << "Body:\n";
   if (func->body == nullptr) { std::cout << pad(depth + 2) << "<null body>\n"; return; }
   print_scope(func->body, depth + 2);
}

void ASTPrinter::print_scope(const NodeScopeBlock* scope, int depth) {
   if (scope == nullptr) { std::cout << pad(depth) << "<null scope>\n"; return; }
   for (const NodeStmt* stmt : scope->stmts) {
      if (stmt == nullptr) { std::cout << pad(depth) << "<null stmt>\n"; continue; }
      print_stmt(stmt, depth);
   }
}

void ASTPrinter::print_stmt(const NodeStmt* stmt, int depth) {
   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeStmtExit* s) {
         std::cout << pad(depth) << "Exit\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtHave* s) {
         std::cout << pad(depth) << "Have: " << s->ident.value.value();
         if (s->resolved.is_array) { 
            std::cout << "     Array type: " 
                      << to_string(Symbols::dt_tok(s->resolved.base)) << std::endl;
         }
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeScopeBlock* s) {
         std::cout << pad(depth) << "Scope\n";
         p->print_scope(s, depth + 1);
      }
      void operator()(const NodeStmtIf* s) {
         std::cout << pad(depth) << "If\n";
         std::cout << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         std::cout << pad(depth + 1) << "Then:\n";
         p->print_scope(s->body, depth + 2);
         if (s->else_body) {
            std::cout << pad(depth + 1) << "Else:\n";
            p->print_scope(s->else_body, depth + 2);
         }
      }
      void operator()(const NodeStmtWhile* s) {
         std::cout << pad(depth) << "While\n";
         std::cout << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         std::cout << pad(depth + 1) << "Body:\n";
         p->print_scope(s->body, depth + 2);
      }
      void operator()(const NodeStmtFor* s) {
         std::cout << pad(depth) << "For\n";
         std::cout << pad(depth + 1) << "Init:\n";
         p->print_stmt(s->init, depth + 2);
         std::cout << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         std::cout << pad(depth + 1) << "Increment:\n";
         p->print_stmt(s->increment, depth + 2);
         std::cout << pad(depth + 1) << "Body:\n";
         p->print_scope(s->body, depth + 2);
      }
      void operator()(const NodeStmtPrint* s) {
         std::cout << pad(depth) << (s->nwln ? "Println:\n" : "Print:\n");
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtAssign* s) {
         std::cout << pad(depth) << "Assign: " << s->ident.value.value() << "\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtReturn* s) {
         std::cout << pad(depth) << "Return\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtExpr* s) {
         std::cout << pad(depth) << "StmtExpr:\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtScope* s) {
         std::cout << pad(depth) << "Scope:\n";
         p->print_scope(s->scope, depth + 1);
      }
   };
   std::visit(Visitor{ this, depth }, stmt->var);
}

void ASTPrinter::print_expr(const NodeExpr* expr, int depth) {
   if (expr == nullptr) { std::cout << pad(depth) << "<null>\n"; return; }

   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeExprIntLit* e) {
         std::cout << pad(depth) << "IntLit: " << e->INT_LIT.value.value() << "\n";
      }
      void operator()(const NodeExprIdent* e) {
         std::cout << pad(depth) << "Ident: " << e->ident.value.value() << "\n";
      }
      void operator()(const NodeBinExpr* e) {
         std::cout << pad(depth) << "BinExpr: " << bin_name(e->operation) << "\n";
         p->print_expr(e->left,  depth + 1);
         p->print_expr(e->right, depth + 1);
      }
      void operator()(const NodeExprIncDec* e) {
         std::cout << pad(depth) << "IncDec:\n" 
                   << pad(depth + 1) << "Ident:  " << e->ident.value.value() << "\n"
                   << pad(depth + 1) << "Mode:   " << (e->is_increment ? "ADD\n" : "SUB\n")
                   << pad(depth + 1) << "Prefix: " << (e->is_prefix ? "True\n" : "False\n");
      }
      void operator()(const NodeExprCall* e) {
         std::cout << pad(depth) << "Call: " << e->name.value.value() << "\n";
         for (const NodeExpr* arg : e->args)
            p->print_expr(arg, depth + 1);
      }
      void operator()(const NodeExprCharLit* e) {
         std::cout << pad(depth) << "CharLit: " << e->CHAR_LIT.value.value() << "\n";
      }

      void operator()(const NodeExprStrLit* str) {
         std::cout << pad(depth) << "StrLit: " << str->STR_LIT.value.value() << "\n";
      }

      void operator()(const NodeExprRead* r) {
         std::cout << pad(depth);
         switch (r->kind) {
            case ReadKind::Char:  std::cout << "Readc\n"; break;
            case ReadKind::Int:   std::cout << "Readi\n"; break;
            case ReadKind::Float: std::cout << "Readf\n"; break;
            case ReadKind::Line:  std::cout << "Reads\n"; break;
            default:              std::cout << "Read<NULL>\n"; break;
         }
      }

      void operator()(const NodeExprArrayLit* a) {
         std::cout << pad(depth) << "Elements:\n";
         for (size_t i = 0; i < a->elements.size(); i++) {
            std::cout << pad(depth) << i << ": ";
            p->print_expr(a->elements.at(i), depth + 1);
         }
      }

      void operator()(const NodeExprIndex* i) {
         std::cout << pad(depth) << "Index Expr:\n";
         std::cout << pad(depth + 1) << "Ident: " << i->ident.value.value() << "\n";
         p->print_expr(i->index, depth + 2);
      }
   };
   std::visit(Visitor{ this, depth }, expr->var);
}

void ASTPrinter::print_condition(const NodeCondition* cond, int depth) {
   if (cond == nullptr) { std::cout << pad(depth) << "<null cond>\n"; return; }

   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeCmpCondition* c) {
         if (c->operation == CmpExprType::NONE) {
            std::cout << pad(depth) << "Cmp: (bare expr, truthy)\n";
            p->print_expr(c->left, depth + 1);
         } else {
            std::cout << pad(depth) << "Cmp: " << cmp_name(c->operation) << "\n";
            p->print_expr(c->left,  depth + 1);
            p->print_expr(c->right, depth + 1);
         }
      }
      void operator()(const NodeLogicCondition* c) {
         std::cout << pad(depth) << "Logic: " << cmp_name(c->operation) << "\n";
         p->print_condition(c->left,  depth + 1);
         p->print_condition(c->right, depth + 1);
      }
   };
   std::visit(Visitor{ this, depth }, cond->var);
}