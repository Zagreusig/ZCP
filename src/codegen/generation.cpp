#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include "generation.h"
#include "parser/parser.h"
#include "lexer/lexer.h"

void ASMGenerator::gen_expr(const NodeExpr* expr) {
   struct ExprVisitor {
      ASMGenerator* gen;

      // void operator()(const NodeUnaryExpr* expr_unary) {
         
      // }

      void operator()(const NodeExprIntLit* expr_int_lit) {
         gen->m_output << "   mov rax, " 
                       << expr_int_lit->INT_LIT.value.value() << '\n';
         gen->push("rax");
      }


      void operator()(const NodeExprIdent* expr_ident) {
         auto var = gen->get_var(expr_ident->ident.value.value());
         if (!var.has_value()) {
            std::cerr << "Undeclared identifier: " << expr_ident->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }

         gen->m_output << "   mov rax, QWORD [rbp + " << var.value().rbp_offset << "]\n";
         gen->push("rax");
      }


      void operator()(const NodeBinExpr* bin_expr) {
         std::visit(ExprVisitor{ .gen = gen }, bin_expr->left->var);
         std::visit(ExprVisitor{ .gen = gen }, bin_expr->right->var);
         gen->pop("rbx");
         gen->pop("rax");

         switch (bin_expr->operation) {
            case BinExprType::ADDITION:
               gen->m_output << "   add rax, rbx\n"; break;
            case BinExprType::SUBTRACTION:
               gen->m_output << "   sub rax, rbx\n"; break;
            case BinExprType::MULTIPLICATION:
               gen->m_output << "   imul rax, rbx\n"; break;
            case BinExprType::DIVISION:
            case BinExprType::MODULUS:
               gen->m_output << "   cqo\n";
               gen->m_output << "   idiv rbx\n";
               if (bin_expr->operation == BinExprType::MODULUS)
                  gen->m_output << "   mov rax, rdx\n";
               break;
            default:
               std::cerr << "Unknown binary operator." << std::endl;
               exit(EXIT_FAILURE);
         }

         gen->push("rax");
      }


      void operator()(const NodeExprCall* call) {
         if (call->args.size() > 6) {
            std::cerr << "Too many arguments in call to \""
                      << call->name.value.value() << "\"" << std::endl;
            exit(EXIT_FAILURE);
         }

         static const std::string param_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
         for (auto arg : call->args)
            gen->gen_expr(arg);
         
         for (int i = call->args.size() - 1; i >= 0; i--)
            gen->pop(param_regs[i]);

         gen->m_output << "   call " << call->name.value.value() << "\n";
         gen->push("rax");
      }

      // have x = 'a';
      void operator()(const NodeExprCharLit* _char) {
         auto var = gen->get_var(_char->CHAR_LIT.value.value());
         if (!var.has_value()) {
            std::cerr << "Undeclared identifier: " << _char->CHAR_LIT.value.value()
                      << std::endl;
            exit(EXIT_FAILURE);
         }

         gen->m_output << "   push QWORD [rbp + " << " bruh " << '\n';
         gen->push("rax");
      }
   };

   ExprVisitor visitor({ .gen = this });
   std::visit(visitor, expr->var);
}


void ASMGenerator::gen_stmt(const NodeStmt* stmt) {
   struct StmtVisitor {
      ASMGenerator* gen;
      void operator()(const NodeStmtExit* stmt_exit) {
         gen->gen_expr(stmt_exit->expr);
         gen->m_output << "   mov rax, 60\n";
         gen->pop("rdi");
         gen->m_output << "   syscall\n";
      }
      
      
      void operator()(const NodeStmtHave* stmt_have) {
         if (gen->get_var(stmt_have->ident.value.value()).has_value()) {
            std::cerr << "Identifier aleady used: " 
                      << stmt_have->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }
         gen->m_current_offset -= 8;
         gen->m_vars.push_back(Var { 
            .name = stmt_have->ident.value.value(), 
            .rbp_offset = gen->m_current_offset 
         });
         gen->gen_expr(stmt_have->expr);
         gen->pop("rax");
         gen->m_output << "   mov QWORD [rbp + " << gen->m_current_offset << "], rax\n";
      }
      
      
      void operator()(const NodeScopeBlock* scope) const {
         gen->begin_scope();
         for (auto stmt : scope->stmts)
            gen->gen_stmt(stmt);
         gen->end_scope();
      }


      void operator()(const NodeStmtIf* _if) const {
         std::string else_label = gen->make_label();
         gen->gen_cond(_if->condition, else_label);

         gen->begin_scope();
         for (auto stmt : _if->body->stmts)
            gen->gen_stmt(stmt);
         gen->end_scope();

         if (_if->else_body != nullptr) {
            std::string end_label = gen->make_label();
            gen->m_output << "   jmp " << end_label << "\n";
            gen->m_output << else_label << ":\n";

            gen->begin_scope();
            for (auto stmt : _if->else_body->stmts)
               gen->gen_stmt(stmt);
            gen->end_scope();
            gen->m_output << end_label << ":\n";
         } else {
            gen->m_output << else_label << ":\n";
         }
      }


      void operator()(const NodeStmtWhile* loop) const {
         std::string strt = gen->make_label();
         std::string ender = gen->make_label();
         gen->m_output << strt << ":\n";
         gen->gen_cond(loop->condition, ender);
         
         gen->begin_scope();
         for (auto stmt : loop->body->stmts)
            gen->gen_stmt(stmt);
         gen->end_scope();

         gen->m_output << "   jmp " << strt << "\n";
         gen->m_output << ender << ":\n";
      }


      void operator()(const NodeStmtAssign* assign) const {
         auto var = gen->get_var(assign->ident.value.value());
         if (!var.has_value()) {
            std::cerr << "Undeclared identifier: " << assign->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }

         gen->gen_expr(assign->expr);
         gen->pop("rax");
         gen->m_output << "   mov QWORD [rbp + " << var.value().rbp_offset << "], rax\n";
      }
   
   
      void operator()(const NodeStmtFor* loop) const {
         gen->begin_scope();

         gen->gen_stmt(loop->init);
         std::string strt = gen->make_label();
         std::string ender = gen->make_label();
         gen->gen_cond(loop->condition, ender);
         gen->m_output << strt << ":\n";
         
         
         gen->begin_scope();
         for (auto stmt : loop->body->stmts)
            gen->gen_stmt(stmt);
         gen->end_scope();
         
         gen->gen_stmt(loop->increment);

         gen->m_output << "   jmp " << strt << "\n";
         gen->m_output << ender << ":\n";

         gen->end_scope();
      }
   


      void operator()(const NodeStmtReturn* _return) const {
         gen->gen_expr(_return->expr);
         gen->pop("rax");
         gen->m_output << "   mov rsp, rbp\n"
                       << "   pop rbp\n"
                       << "   ret\n";
      }
   };

   StmtVisitor visitor { .gen = this };
   std::visit(visitor, stmt->var);
}

void ASMGenerator::gen_cond(const NodeCondition* cond, const std::string& false_label) {
   struct CondVisitor {
      ASMGenerator* gen;
      const std::string& false_label;

      void operator()(const NodeCmpCondition* cmp) {
         if (cmp->operation == CmpExprType::NONE) {
            gen->gen_expr(cmp->left);
            gen->pop("rax");
            gen->m_output << "   cmp rax, 0\n";
            gen->m_output << "   je " << false_label << "\n";
            return;
         }

         gen->gen_expr(cmp->left);
         gen->gen_expr(cmp->right);
         gen->pop("rbx");
         gen->pop("rax");
         gen->m_output << "   cmp rax, rbx\n";
      
         switch (cmp->operation) {
            case CmpExprType::EQUAL:         gen->m_output << "   jne " << false_label << "\n"; break;
            case CmpExprType::NOT_EQUAL:     gen->m_output << "   je "  << false_label << "\n"; break;
            case CmpExprType::LESS_THAN:     gen->m_output << "   jge " << false_label << "\n"; break;
            case CmpExprType::GREATER_THAN:  gen->m_output << "   jle " << false_label << "\n"; break;
            case CmpExprType::LESS_EQUAL:    gen->m_output << "   jg "  << false_label << "\n"; break;
            case CmpExprType::GREATER_EQUAL: gen->m_output << "   jl "  << false_label << "\n"; break;
            default: break;
         }
      }

      void operator()(const NodeLogicCondition* logic) {
         if (logic->operation == CmpExprType::AND) {
            gen->gen_cond(logic->left, false_label);
            gen->gen_cond(logic->right, false_label);
         }
         else if (logic->operation == CmpExprType::OR) {
            std::string true_label = gen->make_label();
            gen->gen_cond_true(logic->left, true_label);
            gen->gen_cond(logic->right, false_label);
            gen->m_output << true_label << ":\n";
         }
      }
   };
   
   std::visit(CondVisitor { .gen = this, .false_label = false_label }, cond->var);
}


void ASMGenerator::gen_cond_true(const NodeCondition* cond, const std::string& true_label) {
   struct CondVisitor {
      ASMGenerator* gen;
      const std::string& true_label;

      void operator()(const NodeCmpCondition* cmp) {
         if (cmp->operation == CmpExprType::NONE) {
            gen->gen_expr(cmp->left);
            gen->pop("rax");
            gen->m_output << "   cmp rax, 0\n";
            gen->m_output << "   jne " << true_label << "\n";
            return;
         }

         gen->gen_expr(cmp->left);
         gen->gen_expr(cmp->right);
         gen->pop("rbx");
         gen->pop("rax");
         gen->m_output << "   cmp rax, rbx\n";

         switch (cmp->operation) {
            case CmpExprType::EQUAL: gen->m_output << "   je " << true_label << "\n"; break;
            case CmpExprType::NOT_EQUAL: gen->m_output << "   jne " << true_label << "\n"; break;
            case CmpExprType::LESS_THAN: gen->m_output << "   jl " << true_label << "\n"; break;
            case CmpExprType::GREATER_THAN: gen->m_output << "   jg " << true_label << "\n"; break;
            case CmpExprType::LESS_EQUAL: gen->m_output << "   jle " << true_label << "\n"; break;
            case CmpExprType::GREATER_EQUAL: gen->m_output << "   jge " << true_label << "\n"; break;
            default: break;
         }
      }

      void operator()(const NodeLogicCondition* logic) {
         if (logic->operation == CmpExprType::OR) {
            gen->gen_cond_true(logic->left, true_label);
            gen->gen_cond_true(logic->right, true_label);
         } else if (logic->operation == CmpExprType::AND) {
            std::string false_label = gen->make_label();
            gen->gen_cond(logic->left, false_label);
            gen->gen_cond(logic->right, false_label);
            gen->m_output << false_label << ":\n";
         }
      }
   };

   std::visit(CondVisitor { .gen = this, .true_label = true_label }, cond->var);
}


void ASMGenerator::gen_function(const NodeFunction* func) {
   m_current_offset = 0;
   int frame_size = compute_frame_size(func->body->stmts);
   frame_size += func->params.size() * 8;
   frame_size = (frame_size + 15) & ~15;
   m_output << "\n\n" << func->name.value.value() << ":\n"
         << "   push rbp\n"
         << "   mov rbp, rsp\n"
         << "   sub rsp, " << frame_size << "\n";


   static const std::string param_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
   for (size_t i = 0; i < func->params.size(); i++) {
      const std::string& name = func->params[i].name.value.value();
      m_current_offset -= 8;
      m_vars.push_back(Var {
         .name = name,
         .rbp_offset = m_current_offset
      });
      m_output << "   mov QWORD [rbp + " << m_current_offset << "], " << param_regs[i] << "\n";
   }
   
   begin_scope();
   for (auto stmt : func->body->stmts)
      gen_stmt(stmt);
   end_scope();

   m_output << "   mov rsp, rbp\n"
            << "   pop rbp\n"
            << "   ret\n";
}



[[nodiscard]] std::string ASMGenerator::build() {
   m_output << "global _start\n_start:\n"
            << "   call main\n   mov rdi, rax\n"
            << "   mov rax, 60\n   syscall\n\n";
   
   for (const NodeFunction* func : m_prog.funcs)
      gen_function(func);

   return m_output.str();
}


void ASMGenerator::begin_scope() {
   m_scope_stack.push_back(m_vars.size());
}


void ASMGenerator::end_scope() {
   size_t var_before = m_scope_stack.back();
   m_scope_stack.pop_back();
   m_vars.resize(var_before);
}


std::optional<ASMGenerator::Var> ASMGenerator::get_var(const std::string& name) {
   for (auto it = m_vars.rbegin(); it != m_vars.rend(); ++it)
      if (it->name == name) return *it;
   return {};
}


int ASMGenerator::count_locals(const NodeScopeBlock* body) {
   int count = 0;
   for (auto stmt : body->stmts) {
      std::visit([&](auto* s) {
         using T = std::decay_t<decltype(*s)>;
         if constexpr (std::is_same_v<T, NodeStmtHave>)
            count++;
         else if constexpr (std::is_same_v<T, NodeScopeBlock>)
            count += count_locals(s);
         else if constexpr (std::is_same_v<T, NodeScopeBlock>) {
            count += count_locals(s->body);
            if (s->else_body) count += count_locals(s->else_body);
         }
         else if constexpr (std::is_same_v<T, NodeStmtWhile>)
            count += count_locals(s->body);
         else if constexpr (std::is_same_v<T, NodeStmtFor>) {
            count++;
            count += count_locals(s->body);
         }
      }, stmt->var);
   }

   return count;
}


int ASMGenerator::count_locals(const std::vector<NodeStmt*>& stmts) {
   int count = 0;
   for (auto stmt : stmts) {
      std::visit([&](auto* s) {
         using T = std::decay_t<decltype(*s)>;
         if constexpr (std::is_same_v<T, NodeStmtHave>)
            count++;
         else if constexpr (std::is_same_v<T, NodeScopeBlock>)
            count += count_locals(s);
         else if constexpr (std::is_same_v<T, NodeScopeBlock>) {
            count += count_locals(s->body);
            if (s->else_body) count += count_locals(s->else_body);
         }
         else if constexpr (std::is_same_v<T, NodeStmtWhile>)
            count += count_locals(s->body);
         else if constexpr (std::is_same_v<T, NodeStmtFor>) {
            count++;
            count += count_locals(s->body);
         }
      }, stmt->var);
   }

   return count;
}

int ASMGenerator::compute_frame_size(const NodeScopeBlock* body) {
   int locals = count_locals(body);
   int bytes = locals * 8;
   return (bytes + 15) & ~15; // Rounding up to 16-byte alignment
}


int ASMGenerator::compute_frame_size(const std::vector<NodeStmt*>& stmts) {
   int locals = count_locals(stmts);
   int bytes = locals * 8;
   return (bytes + 15) & ~15;
}