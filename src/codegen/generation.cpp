#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include "ast/Nodes.h"
#include "type_checker.h"
#include "generation.h"
#include "lexer/Tokens.h"

/**
 *  rax: 0 read
 *       1 write
 * 
 *  rdi: 0 stdin
 *       1 stdout 
 */

void ASMGenerator::gen_expr(const NodeExpr* expr) {
   struct ExprVisitor {
      ASMGenerator* gen;

      void operator()(const NodeExprIntLit* expr_int_lit) {
         gen->m_output << "   mov rax, " 
                       << expr_int_lit->INT_LIT.value.value() << '\n';
         gen->push("rax");
      }


      void operator()(const NodeExprCharLit* _char) {
         gen->m_output << "   mov rax, '" << _char->CHAR_LIT.value.value() << "'\n";
         gen->push("rax");         
      }


      void operator()(const NodeExprStrLit* _str) {
         std::cerr << "String values currently only work as args." << std::endl;
         exit(EXIT_FAILURE);
      }


      void operator()(const NodeExprIdent* expr_ident) {
         auto var = gen->get_var(expr_ident->ident.value.value());
         if (!var.has_value()) {
            std::cerr << "Undeclared identifier visiting during gen_expr: " << expr_ident->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }

         gen->m_output << "   mov rax, QWORD [rbp + " << var.value().rbp_offset << "]\n";
         gen->push("rax");
      }


      void operator()(const NodeExprRead* read) {
         switch (read->kind) {
            case ReadKind::Char:  gen->m_output << "   call read_char\n"; break;
            case ReadKind::Int:   gen->m_output << "   call read_int\n";  break;
            case ReadKind::Float: 
            case ReadKind::Line:
            default:              std::cerr << "Invalid read statement.\n"; exit(EXIT_FAILURE);
         }
         gen->push("rax");
      }


      void operator()(const NodeExprIncDec* node) {
         auto var = gen->get_var(node->ident.value.value());
         if (!var.has_value()) {
            std::cerr << "Undeclared Identifier IncDec: " << node->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }
         std::string slot = "QWORD [rbp + " + std::to_string(var.value().rbp_offset) + "]";
         const char* op = node->is_increment ? "add" : "sub";

         if (node->is_prefix) {
            gen->m_output << "   " << op << " " << slot << ", 1\n"; // Mod first
            gen->m_output << "   mov rax, " << slot << "\n";        // use new val
            gen->push("rax");
         } else {
            gen->m_output << "   mov rax, " << slot << "\n";       // Save to rax
            gen->push("rax");
            gen->m_output << "   " << op << " " << slot << ", 1\n"; // Now mod
         }
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


      void operator()(const NodeExprArrayLit* arr) {
         return;
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
         gen->m_output << "   mov rax, SYS_exit\n";
         gen->pop("rdi");
         gen->m_output << "   syscall\n";
      }

      
      void operator()(const NodeStmtExpr* s) {
         gen->gen_expr(s->expr);
         gen->pop("rax");
      }
      
      
      void operator()(const NodeStmtHave* stmt_have) {
         if (gen->get_var(stmt_have->ident.value.value()).has_value()) {
            std::cerr << "Identifier aleady used (have): " 
                      << stmt_have->ident.value.value() << std::endl;
            exit(EXIT_FAILURE);
         }
         
         const TypeInfo& type = stmt_have->resolved;
         if (type.is_array) {
            int total = type.byte_size();
            int esz   = type.elem_size();
            gen->m_current_offset -= total;
            int base = gen->m_current_offset;
            gen->m_vars.push_back(Var { .name = stmt_have->ident.value.value(), .rbp_offset = base, 
                                        .type = type });
            gen->m_types.declare_var(stmt_have->ident.value.value(), type.base);
         
            if (stmt_have->expr) {
               auto* lit = std::get_if<NodeExprArrayLit*>(&stmt_have->expr->var);
               const auto& elems = (*lit)->elements;
               for (size_t i = 0; i < elems.size(); i++) {
                  gen->gen_expr(elems[i]);
                  gen->pop("rax");
                  int off = base + (int)i * esz;
                  if (esz == 8) gen->m_output << "   mov QWORD [rbp + " << off  << "], rax\n";
                  else          gen->m_output << "   mov byte [rbp + " << off << "], al\n";
               }
            } else {
               // sized, uninit: zero entire region of mem
               for (int i = 0; i < type.array_len; i++) {
                  int off = base + i * esz;
                  if (esz == 8) gen->m_output << "   mov QWORD [rbp + " << off << "], 0\n";
                  else          gen->m_output << "   mov byte [rbp + "  << off << "], 0\n";
               }
            }
            return;
         }

         
         gen->m_current_offset -= 8; // still 8 bytes for now. (will update later)
         int off = gen->m_current_offset;
         gen->m_vars.push_back(Var { 
            .name = stmt_have->ident.value.value(), 
            .rbp_offset = off, .type = type 
         });
         gen->m_types.declare_var(stmt_have->ident.value.value(), type.base);
         
         if (stmt_have->expr) {
            gen->gen_expr(stmt_have->expr);
            gen->pop("rax");
            gen->m_output << "   mov QWORD [rbp + " << off << "], rax\n";
         } else {
            gen->m_output << "   mov QWORD [rbp + " << off << "], 0\n";
         }
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
            std::cerr << "Undeclared identifier Assign: " << assign->ident.value.value() << std::endl;
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


      void operator()(const NodeStmtScope* block) const {
         gen->begin_scope();
         for (auto stmt : block->scope->stmts)
            gen->gen_stmt(stmt);
         gen->end_scope();
      }


      /**
       * 64-bit write registers:
       * rax = 1      <- syscall code for write
       * rdi = 1      <- fd (stdout)
       * rsi = buffer <- pointer to the bytes
       * rdx = length <- how many bytes
       */
      void operator()(const NodeStmtPrint* p) const {
         DataType t = gen->m_types.type_of(p->expr);
         std::cout << "Checked type: " << (int)t << std::endl;
         switch (t) {
            case DataType::INT:
               gen->gen_expr(p->expr);
               gen->pop("rax");
               gen->m_output << "   mov rdi, " << (p->nwln ? 1 : 0) << '\n';
               gen->m_output << "   call print_int\n";
               break;
            case DataType::STR: {
               auto* str_node = std::get_if<NodeExprStrLit*>(&p->expr->var);
               if (!str_node) {
                  std::cerr << "Print works with string literals currently." << std::endl;
                  exit(EXIT_FAILURE);
               }

               const std::string& bytes = (*str_node)->STR_LIT.value.value();
               std::string label = gen->add_string(bytes);

               gen->m_output << "   mov rax, SYS_write\n"
                             << "   mov rdi, STDOUT\n"
                             << "   mov rsi, " << label << '\n'
                             << "   mov rdx, " << label << "_len\n"
                             << "   syscall\n";

               if (p->nwln) {
                  gen->m_output << "   mov byte [print_buf], LF\n"
                                << "   mov rax, SYS_write\n"
                                << "   mov rdi, STDOUT\n"
                                << "   mov rsi, print_buf\n"
                                << "   mov rdx, 1\n"
                                << "   syscall\n";
               }
               break;
            }
            case DataType::CHAR:
               gen->gen_expr(p->expr);
               gen->pop("rax");  // char sits in al (low byte of rax)

               if (!p->nwln) {
                  gen->m_output << "   mov byte [print_buf], al\n";
                  gen->m_output << "   mov rsi, print_buf\n"
                                << "   mov rdx, 1\n"
                                << "   mov rax, SYS_write\n"
                                << "   mov rdi, STDOUT\n"
                                << "   syscall\n";
               } else {
                  gen->m_output << "   mov byte [print_buf], al\n"
                                << "   mov byte [print_buf + 1], LF\n"
                                << "   mov rsi, print_buf\n"
                                << "   mov rdx, 2\n"
                                << "   mov rdi, STDOUT\n"
                                << "   mov rax, SYS_write\n"
                                << "   syscall\n";
               }
               break;
            default: 
               std::cerr << "Cannot print value of unknown type." << std::endl;
               exit(EXIT_FAILURE);
         }
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
            gen->m_output << "   cmp rax, FALSE\n";
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
            gen->m_output << "   cmp rax, FALSE\n";
            gen->m_output << "   jne " << true_label << "\n";
            return;
         }

         gen->gen_expr(cmp->left);
         gen->gen_expr(cmp->right);
         gen->pop("rbx");
         gen->pop("rax");
         gen->m_output << "   cmp rax, rbx\n";

         switch (cmp->operation) {
            case CmpExprType::EQUAL: gen->m_output         << "   je "  << true_label << "\n"; break;
            case CmpExprType::NOT_EQUAL: gen->m_output     << "   jne " << true_label << "\n"; break;
            case CmpExprType::LESS_THAN: gen->m_output     << "   jl "  << true_label << "\n"; break;
            case CmpExprType::GREATER_THAN: gen->m_output  << "   jg "  << true_label << "\n"; break;
            case CmpExprType::LESS_EQUAL: gen->m_output    << "   jle " << true_label << "\n"; break;
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

   begin_scope();
   static const std::string param_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
   for (size_t i = 0; i < func->params.size(); i++) {
      const std::string& name = func->params[i].name.value.value();
      m_current_offset -= 8;
      m_vars.push_back(Var {
         .name = name,
         .rbp_offset = m_current_offset
      });
      m_types.declare_var(name, map_token_type(func->params[i].type.type));
      m_output << "   mov QWORD [rbp + " << m_current_offset << "], " << param_regs[i] << "\n";
   }
   

   for (auto stmt : func->body->stmts)
      gen_stmt(stmt);
   end_scope();

   m_output << "   mov rsp, rbp\n"
            << "   pop rbp\n"
            << "   ret\n";
}



[[nodiscard]] std::string ASMGenerator::build() {   
   emit_consts();
   m_output << "section .bss\n"
            << "   print_buf resb 32\n"
            << "   read_buf  resb 64\n"
            << "   chr       resb 1\n\n"
            << "section .text\n"
            << "global _start\n_start:\n"
            << "   call main\n   mov rdi, rax\n"
            << "   mov rax, SYS_exit\n   syscall\n\n";

   emit_print_int();
   emit_read_int();
   emit_read_char();
   
   for (const NodeFunction* func : m_prog.funcs)
      gen_function(func);

   if (m_strings.empty()) return m_output.str();

   m_output << "\n\nsection .data\n";
   for (const auto& [label, bytes]: m_strings) {
      m_output << "   " << label << ": db ";
      for (size_t i = 0; i < bytes.size(); i++) {
         if (i) m_output << ", ";
         m_output << (int)(unsigned char)bytes[i];
      }

      if (bytes.empty()) m_output << "NULL";
      m_output << "\n"
               << "   " << label << "_len: equ $ - " << label << "\n";
   }

   return m_output.str();
}


void ASMGenerator::begin_scope() {
   m_scope_stack.push_back(m_vars.size());
   m_types.push_scope();
}


void ASMGenerator::end_scope() {
   size_t var_before = m_scope_stack.back();
   m_scope_stack.pop_back();
   m_vars.resize(var_before);
   m_types.pop_scope();
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
         else if constexpr (std::is_same_v<T, NodeStmtIf>) {
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


int ASMGenerator::have_byte_size(const NodeStmtHave* h) {
   /* WIP */
   return 8;
}


int ASMGenerator::compute_frame_size(const std::vector<NodeStmt*>& stmts) {
   int bytes = 0;
   for (auto stmt : stmts) {
      std::visit([&](auto* s) {
         using T = std::decay_t<decltype(*s)>;
         if constexpr (std::is_same_v<T, NodeStmtHave>) {
            bytes += s->resolved.byte_size();
         }
         else if constexpr (std::is_same_v<T, NodeStmtWhile>) 
            bytes += compute_frame_size(s->body->stmts);
         else if constexpr (std::is_same_v<T, NodeStmtFor>)
            bytes += compute_frame_size(s->body->stmts);
         else if constexpr (std::is_same_v<T, NodeStmtIf>) {
            bytes += compute_frame_size(s->body->stmts);
            if (s->else_body) bytes += compute_frame_size(s->else_body->stmts);
         }
         else if constexpr (std::is_same_v<T, NodeScopeBlock>)
            bytes += compute_frame_size(s->stmts);
      }, stmt->var);
   }
   return bytes;
}


/** TODO: Remove this lol */
int ASMGenerator::console_write(const std::string& msg) {
   if (msg.empty()) return -1;
   int len = msg.length();
   m_output << "   mov eax, 1\n"
            << "   mov ebx, 1\n"
            << "   mov ecx, \"" << msg << "\"\n"
            << "   mov edx, " << len << '\n'
            << "   syscall\n";
   return 0;
}


void ASMGenerator::emit_consts() {
   m_output << "LF           equ 10     ; newline\n"
            << "NULL         equ 0      ; null term\n"
            << "TRUE         equ 1\n"
            << "FALSE        equ 0\n\n"
            << "EXIT_SUCCESS equ 0\n\n"
            << "STDIN        equ 0\n"
            << "STDOUT       equ 1\n"
            << "STDERR       equ 2\n\n"
            << "SYS_read     equ 0\n"
            << "SYS_write    equ 1\n"
            << "SYS_open     equ 2     ; file open\n"
            << "SYS_close    equ 3     ; file close\n"
            << "SYS_fork     equ 57    ; fork\n"
            << "SYS_exit     equ 60    ; terminate\n"
            << "SYS_creat    equ 85    ; file open/create\n"
            << "SYS_time     equ 201   ; get time\n\n"
            << "STDIN_LEN    equ 64    ; max len to read\n"
            << "newLine      db  LF, NULL\n";
}



void ASMGenerator::emit_print_int() {
   m_output << 
"print_int:                 ; Positive ints for now\n\
   ; rax = num, rdi = newline flag (0/1)\n\
   mov byte [print_buf + 31], LF   ; park '\\n' at the end\n\
   lea rsi, [print_buf + 30]       ; digits fill from here back\n\
   mov rcx, LF\n\
   mov r8, 0\n\
   test rax, rax\n\
   jnz .pi_sign\n\
   mov byte [rsi], '0'\n\
   dec rsi\n\
   jmp .pi_write\n\
.pi_sign:\n\
   jns .pi_convert\n\
   mov r8, 1\n\
   neg rax\n\
.pi_convert:\n\
   xor rdx, rdx\n\
   div rcx\n\
   add dl, '0'\n\
   mov [rsi], dl\n\
   dec rsi\n\
   test rax, rax\n\
   jnz .pi_convert\n\
   cmp r8, 1\n\
   jne .pi_write\n\
   mov byte [rsi], '-'\n\
   dec rsi\n\
.pi_write:\n\
   inc rsi\n\
   ; base length: up to the byte BEFORE newline slot\n\
   lea rdx, [print_buf + 31]\n\
   sub rdx, rsi    ; length WITHOUT newline\n\
   add rdx, rdi    ; + flag (if using newline, adds the byte back)\n\
   mov rax, SYS_write\n\
   mov rdi, STDOUT ; NOTE: this overwrites flag.\n\
   syscall\n\
   ret\n";
}


void ASMGenerator::emit_read_chars() {
}


void ASMGenerator::emit_read_char() {
   m_output << "read_char:\n"
            << "   mov rax, SYS_read\n"
            << "   mov rdi, STDIN\n"
            << "   lea rsi, [chr]\n"
            << "   mov rdx, 1\n"
            << "   syscall\n"
            << "   movzx rax, byte [chr]\n"
            << "   ret\n\n";
}


void ASMGenerator::emit_read_int() {
   m_output << "read_int:\n"
            << "   push rbx\n"
            << "   push r12\n"
            << "   push r13\n\n"
            << "   xor rax, rax          ; accumulator = 0\n"
            << "   xor r13, r13          ; sign = 0 (pos)\n"
            << "   mov r12, 0            ; dig count guard\n\n"
            << ".ri_loop:\n"
            << "   push rax\n"
            << "   mov rax, SYS_read\n"
            << "   mov rdi, STDIN\n"
            << "   lea rsi, [chr]\n"
            << "   mov rdx, 1\n"
            << "   syscall\n"
            << "   cmp rax, FALSE\n"
            << "   pop rax\n"
            << "   je .ri_done           ; EOF -> stop\n\n"
            << "   movzx rcx, byte [chr] ; the char, zero-extended (ONE byte)\n"
            << "   cmp rcx, LF\n"
            << "   je .ri_done\n"
            << "   cmp rcx, '0'\n"
            << "   jb .ri_done\n"
            << "   cmp rcx, '9'\n"
            << "   ja .ri_done\n\n"
            << "   sub rcx, '0'          ; ASCII -> digit value\n"
            << "   mov rdx, rax\n"
            << "   shl rax, 3            ; rax * 8\n"
            << "   shl rdx, 1            ; rdx * 2\n"
            << "   add rax, rdx          ; rax * 10\n"
            << "   add rax, rcx          ; + digit\n"
            << "   jmp .ri_loop\n\n"
            << ".ri_done:\n"
            << "   pop r13\n"
            << "   pop r12\n"
            << "   pop rbx\n"
            << "   ret\n\n";

}


std::string ASMGenerator::add_string(const std::string& str) {
   std::string label = "str_" + std::to_string(m_str_count++);
   m_strings.push_back({ label, str });
   return label;
}


TypeInfo ASMGenerator::resolve_have_type(NodeStmtHave* h) {
   if (h->is_resolved) return h->resolved;

   TypeInfo info;
   if (h->has_type)
      info = h->decl_type;
   else if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->var)) {
      const auto& elems = (*lit)->elements;
      if (elems.empty()) { /* error: empty lit can't infer*/ }
      DataType et = m_types.type_of(elems[0]);
      for (size_t i = 1; i < elems.size(); i++) {
         if (m_types.type_of(elems[i]) != et) {
            std::cerr << "Array literal has mismatched element types." << std::endl;
            exit(EXIT_FAILURE);
         }
      }

      info.base = et;
      info.is_array = true;
      info.array_len = elems.size();
   }
   else info.base = m_types.type_of(h->expr); // scalar inference

   h->resolved = info;
   h->is_resolved = true;
   return info;
}