#include "ASTPrinter.h"

#include <stddef.h>
#include <string_view>
#include <variant>

#include "Core/EscapeChars.h"
#include "Core/SymbolTable.h"
#include "Nodes.h"
#include "TokenTable.h"
#include "Tokens.h"

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
   if (func == nullptr) { m_out << pad(depth) << "<null func>\n"; return; }
   m_out << pad(depth) << "Function: " << func->name.text();
   if (func->has_ret_type)
      m_out << " -> " << to_string(func->ret_type.type);
   m_out << "\n";

   if (!func->params.empty()) {
      m_out << pad(depth + 1) << "Params:\n";
      for (const NodeParam& p : func->params)
         m_out << pad(depth + 2)
                   << Symbols::dt_str(p.type.base) << " "
                   << p.name.text() << "\n";
   }

   m_out << pad(depth + 1) << "Body:\n";
   if (func->body == nullptr) { m_out << pad(depth + 2) << "<null body>\n"; return; }
   print_scope(func->body, depth + 2);
}

void ASTPrinter::print_scope(const NodeScopeBlock* scope, int depth) {
   if (scope == nullptr) { m_out << pad(depth) << "<null scope>\n"; return; }
   for (const NodeStmt* stmt : scope->stmts) {
      if (stmt == nullptr) { m_out << pad(depth) << "<null stmt>\n"; continue; }
      print_stmt(stmt, depth);
   }
}

void ASTPrinter::print_stmt(const NodeStmt* stmt, int depth) {
   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeStmtExit* s) {
         p->m_out << pad(depth) << "Exit\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtHave* s) {
         p->m_out << pad(depth) << "Have: " << s->ident.text();
         if (s->resolved.is_array) { 
            p->m_out << "     Array type: " 
                      << to_string(Symbols::dt_tok(s->resolved.base)) << std::endl;
         }
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeScopeBlock* s) {
         p->m_out << pad(depth) << "Scope\n";
         p->print_scope(s, depth + 1);
      }
      void operator()(const NodeStmtIf* s) {
         p->m_out << pad(depth) << "If\n";
         p->m_out << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         p->m_out << pad(depth + 1) << "Then:\n";
         p->print_scope(s->body, depth + 2);
         if (s->else_body) {
            p->m_out << pad(depth + 1) << "Else:\n";
            p->print_scope(s->else_body, depth + 2);
         }
      }
      void operator()(const NodeStmtWhile* s) {
         p->m_out << pad(depth) << "While\n";
         p->m_out << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         p->m_out << pad(depth + 1) << "Body:\n";
         p->print_scope(s->body, depth + 2);
      }
      void operator()(const NodeStmtFor* s) {
         p->m_out << pad(depth) << "For\n";
         p->m_out << pad(depth + 1) << "Init:\n";
         p->print_stmt(s->init, depth + 2);
         p->m_out << pad(depth + 1) << "Condition:\n";
         p->print_condition(s->condition, depth + 2);
         p->m_out << pad(depth + 1) << "Increment:\n";
         p->print_stmt(s->increment, depth + 2);
         p->m_out << pad(depth + 1) << "Body:\n";
         p->print_scope(s->body, depth + 2);
      }
      void operator()(const NodeStmtPrint* s) {
         p->m_out << pad(depth) << (s->nwln ? "Println:\n" : "Print:\n");
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtAssign* s) {
         p->m_out << pad(depth) << "Assign: " << s->ident.text() << "\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtReturn* s) {
         p->m_out << pad(depth) << "Return\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtExpr* s) {
         p->m_out << pad(depth) << "StmtExpr:\n";
         p->print_expr(s->expr, depth + 1);
      }
      void operator()(const NodeStmtScope* s) {
         p->m_out << pad(depth) << "Scope:\n";
         p->print_scope(s->scope, depth + 1);
      }
   };
   std::visit(Visitor{ this, depth }, stmt->variant);
}

void ASTPrinter::print_expr(const NodeExpr* expr, int depth) {
   if (expr == nullptr) { m_out << pad(depth) << "<null>\n"; return; }

   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeExprIntLit* e) {
         p->m_out << pad(depth) << "IntLit: " << e->INT_LIT.int_val() << "\n";
      }
      void operator()(const NodeExprIdent* e) {
         p->m_out << pad(depth) << "Ident: " << e->ident.text() << "\n";
      }
      void operator()(const NodeBinExpr* e) {
         p->m_out << pad(depth) << "BinExpr: " << bin_name(e->operation) << "\n";
         p->print_expr(e->left,  depth + 1);
         p->print_expr(e->right, depth + 1);
      }
      void operator()(const NodeExprIncDec* e) {
         p->m_out << pad(depth) << "IncDec:\n" 
                   << pad(depth + 1) << "Ident:  " << e->ident.text() << "\n"
                   << pad(depth + 1) << "Mode:   " << (e->is_increment ? "ADD\n" : "SUB\n")
                   << pad(depth + 1) << "Prefix: " << (e->is_prefix ? "True\n" : "False\n");
      }
      void operator()(const NodeExprCall* e) {
         p->m_out << pad(depth) << "Call: " << e->name.text() << "\n";
         for (const NodeExpr* arg : e->args)
            p->print_expr(arg, depth + 1);
      }
      void operator()(const NodeExprCharLit* e) {
         if (Esc::is_esc_char(e->CHAR_LIT.char_val()))
            p->m_out << pad(depth) << "CharLit: " << Esc::esc_str(e->CHAR_LIT.char_val()) << "\n";
         else
            p->m_out << pad(depth) << "CharLit: " << e->CHAR_LIT.char_val() << "\n";
      }

      void operator()(const NodeExprStrLit* str) {
         p->m_out << pad(depth) << "StrLit: " << str->STR_LIT.text() << "\n";
      }

      void operator()(const NodeExprRead* r) {
         p->m_out << pad(depth);
         switch (r->kind) {
            case ReadKind::Char:  p->m_out << "Readc\n"; break;
            case ReadKind::Int:   p->m_out << "Readi\n"; break;
            case ReadKind::Float: p->m_out << "Readf\n"; break;
            case ReadKind::Line:  p->m_out << "Reads\n"; break;
            default:              p->m_out << "Read<NULL>\n"; break;
         }
      }

      void operator()(const NodeExprArrayLit* a) {
         p->m_out << pad(depth) << "Elements:\n";
         for (size_t i = 0; i < a->elements.size(); i++) {
            p->m_out << pad(depth) << i << ": ";
            p->print_expr(a->elements.at(i), depth + 1);
         }
      }

      void operator()(const NodeExprIndex* i) {
         p->m_out << pad(depth) << "Index Expr:\n";
         p->m_out << pad(depth + 1) << "Ident: " << i->ident.text() << "\n";
         p->print_expr(i->index, depth + 2);
      }
   };
   std::visit(Visitor{ this, depth }, expr->variant);
}

void ASTPrinter::print_condition(const NodeCondition* cond, int depth) {
   if (cond == nullptr) { m_out << pad(depth) << "<null cond>\n"; return; }

   struct Visitor {
      ASTPrinter* p;
      int depth;

      void operator()(const NodeCmpCondition* c) {
         if (c->operation == CmpExprType::NONE) {
            p->m_out << pad(depth) << "Cmp: (bare expr, truthy)\n";
            p->print_expr(c->left, depth + 1);
         } else {
            p->m_out << pad(depth) << "Cmp: " << cmp_name(c->operation) << "\n";
            p->print_expr(c->left,  depth + 1);
            p->print_expr(c->right, depth + 1);
         }
      }
      void operator()(const NodeLogicCondition* c) {
         p->m_out << pad(depth) << "Logic: " << cmp_name(c->operation) << "\n";
         p->print_condition(c->left,  depth + 1);
         p->print_condition(c->right, depth + 1);
      }
   };
   std::visit(Visitor{ this, depth }, cond->var);
}