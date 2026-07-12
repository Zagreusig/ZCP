#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <type_traits>
#include <variant>

#include "Core/Nodes.h"
#include "generation.h"
#include "Core/Tokens.h"
#include "Core/EscapeChars.h"
#include "Core/SymbolTable.h"
#include "utils/msc.h"
#include "TokenTable.h"


void ASMGenerator::gen_expr(const NodeExpr* expr) {
   struct ExprVisitor {
      ASMGenerator* gen;

      void operator()(const NodeExprIntLit* expr_int_lit) {
         gen->m_output << "   mov rax, " 
                       << expr_int_lit->INT_LIT.int_val() << '\n';
         gen->push("rax");
      }


      void operator()(const NodeExprCharLit* _char) {
         const char ch = _char->CHAR_LIT.char_val();
         if (Esc::is_esc_char(ch))
            gen->m_output << "   mov rax, " << Esc::asm_code(ch) << "\n";
         else {
            gen->m_output << "   mov rax, '";
            if (ch == '\'')
               gen->m_output << "\\";
            gen->m_output << ch << "'\n";
         }
         gen->push("rax");         
      }


      void operator()(const NodeExprStrLit* _str) {
         std::cerr << "DEBUG: " << _str->STR_LIT.text() << std::endl;
         
         std::cerr << "String values currently only work as args." << std::endl;
         gen->total_fail();
      }


      void operator()(const NodeExprIdent* expr_ident) {
         auto var = gen->get_var(expr_ident->ident.text());
         if (!var.has_value()) {
            std::cerr << "Undeclared identifier visiting during gen_expr: " << expr_ident->ident.text() << std::endl;
            gen->total_fail();
         }

         const auto& v = var.value();
         if (v.type.base == DataType::CHAR)
            gen->m_output << gen->load_scalar(v.offset, "movzx", "rax", "byte");
         else
            gen->m_output << gen->load_scalar(v.offset, "mov", "rax", "QWORD");
         gen->push("rax");
      }


      // r12 is frame base
      // rax holds base_offset + index*esz
      // [r12 + rax] = element's address
      void operator()(const NodeExprIndex* idx) {
         auto var = gen->get_var(idx->ident.text());
         if (!var.has_value()) {
            std::cerr << "Undeclared array: " << idx->ident.text() << std::endl;
            gen->total_fail();
         }
         
         if (var.value().type.base == DataType::STR) {
            gen->gen_expr(idx->index);
            gen->pop("rbx");
            gen->m_output << "   mov rax, QWORD [r12 + " << var.value().offset << "]\n"
                          << "   movzx rax, byte [rax + rbx]\n";
            gen->push("rax");
            return;
         }

         int esz = var.value().type.elem_size();

         // eval expr -> its value on the stack
         gen->gen_expr(idx->index);
         gen->pop("rax"); // rax = index value

         // address = r12 + base_offset + (index * esz)
         //   comp elem offset into rax, then load from [r12 + rax + base]
         if (esz != 1) 
            gen->m_output << "   imul rax, " << esz << "\n";    // rax = index * elem_size
         gen->m_output << "   add rax, " << var.value().offset << "\n"; // + base offset

         // load element
         if (esz == 8) {
            gen->m_output << "   mov rax, QWORD [r12 + rax]\n";
         }
         else // byte element
            gen->m_output << "   movzx rax, byte [r12 + rax]\n";

         gen->push("rax");
      }


      void operator()(const NodeExprRead* read) {
         switch (read->kind) {
            case ReadKind::Char:  gen->m_output << "   call read_char\n"; break;
            case ReadKind::Int:   gen->m_output << "   call read_int\n";  break;
            case ReadKind::Float: 
            case ReadKind::Line:
            default:              std::cerr << "Invalid read statement.\n"; gen->total_fail();
         }
         gen->push("rax");
      }


      void operator()(const NodeExprIncDec* node) {
         auto var = gen->get_var(node->ident.text());
         if (!var.has_value()) {
            std::cerr << "Undeclared Identifier IncDec: " << node->ident.text() << std::endl;
            gen->total_fail();
         }
         std::string slot = "QWORD [r12 + " + std::to_string(var.value().offset) + "]";
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
         gen->gen_expr(bin_expr->left);
         
         if (gen->try_load_simple(bin_expr->right, "rbx"))
            gen->pop("rax");
         else {
            gen->gen_expr(bin_expr->right);
            gen->pop("rax");
            gen->pop("rbx");
         }
         switch (bin_expr->operation) {
            case BinExprType::ADDITION:       gen->m_output << "   add rax, rbx\n"; break;
            case BinExprType::SUBTRACTION:    gen->m_output << "   sub rax, rbx\n"; break;
            case BinExprType::MULTIPLICATION: gen->m_output << "   imul rax, rbx\n"; break;
            case BinExprType::DIVISION:
            case BinExprType::MODULUS:
               gen->m_output << "   cqo\n";
               gen->m_output << "   idiv rbx\n";
               if (bin_expr->operation == BinExprType::MODULUS)
                  gen->m_output << "   mov rax, rdx\n";
               break;
            default:
               std::cerr << "Unknown binary operator." << std::endl;
               gen->total_fail();
         }

         gen->push("rax");
      }


      void operator()(const NodeExprCall* call) {
         // comp total reg footprint, check <= 6.
         // then load each into indiv reg slot(s)

         static const std::string param_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
         int reg_idx = 0;

         for (auto* arg : call->args) {
            const TypeInfo& at = arg->resolved;
            int regs_used = gen->param_reg_count(at);

            if (at.base == DataType::STR) {
               // load ptr & len in 2 consec regs
               if (auto* id = std::get_if<NodeExprIdent*>(&arg->variant)) {
                  auto var = gen->get_var((*id)->ident.text());
                  int off = var.value().offset;
                  gen->m_output << "   mov " << param_regs[reg_idx] << ", QWORD [r12 + " << off << "]\n"
                                << "   mov " << param_regs[reg_idx + 1] << ", QWORD [r12 + " << (off + 8) << "]\n";
               }
               else if (auto* lit = std::get_if<NodeExprStrLit*>(&arg->variant)) {
                  const std::string& str = (*lit)->STR_LIT.text();
                  std::string label = gen->add_string(str);
                  gen->m_output << "   lea " << param_regs[reg_idx] << ", [" << label << "]\n"
                                << "   mov " << param_regs[reg_idx + 1] << ", " << str.size() << "\n";
               }
            } else {
               // scalar arg: eval into rax, move into reg
               gen->gen_expr(arg);
               gen->pop(param_regs[reg_idx]);
            }
            reg_idx += regs_used;
         }
         gen->m_output << "   call " << call->name.text() << "\n";
         
         auto it = gen->m_funcs.find(call->name.text());
         bool returns_str = (it != gen->m_funcs.end() && it->second.base == DataType::STR);
         if (!returns_str)
            gen->push("rax"); // ret val
         // if returning str, leave rax:rdx alone for caller to deal with.
      }


      void operator()(const NodeExprArrayLit* arr) {
         return;
      }
   };

   ExprVisitor visitor({ .gen = this });
   std::visit(visitor, expr->variant);
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
      
      
      void operator()(const NodeStmtHave* h) {
         if (gen->get_var(h->ident.text()).has_value()) {
            std::cerr << "Identifier aleady used (have): " 
                      << h->ident.text() << std::endl;
            gen->total_fail();
         }
         
         const TypeInfo& type = h->resolved;
         if (type.base == DataType::STR) {
            int base = gen->m_current_offset;
            gen->m_vars.push_back(Var{ .name = h->ident.text(), .offset = base, .type = type });
            gen->m_current_offset += 16; // fat pointer
         
            if (h->expr) {
               if (auto* lit = std::get_if<NodeExprStrLit*>(&h->expr->variant))
                  gen->store_string_literal(base, (*lit)->STR_LIT.text());
               else if (auto* call = std::get_if<NodeExprCall*>(&h->expr->variant)) {
                  // call returns a string in rax:rdx; stor into the slot.
                  gen->gen_expr(h->expr);
                  gen->m_output << "   mov QWORD [r12 + " << base << "], rax\n"
                                << "   mov QWORD [r12 + " << (base + 8) << "], rdx\n";
               }
               else if (auto* var = std::get_if<NodeExprIdent*>(&h->expr->variant)) {
                  if (h->expr->resolved.base != DataType::STR) {
                     std::cerr << "Cannot initialize type string with other type.\n";
                     gen->total_fail();
                  }

                  auto id = gen->get_var((*var)->ident.text());
                  if (!id.has_value()) {
                     std::cerr << "Undefined identifier '" << (*var)->ident.text() << "'\n";
                     gen->total_fail();
                  }
                  int ref_off = id.value().offset;
                  gen->m_output << "   mov rax, QWORD [r12 + " << ref_off << "]\n"         // load other's ptr
                                << "   mov QWORD [r12 + " << base << "], rax\n"            // store to target's ptr  
                                << "   mov rax, QWORD [r12 + " << (ref_off + 8) << "]\n"   // load other's len
                                << "   mov QWORD [r12 + " << (base + 8) << "], rax\n";     // store to target's len
               }
            } else {
               // uninit: nullptr, len 0
               gen->m_output << "   mov QWORD [r12 + " << base << "], 0\n"
                             << "   mov QWORD [r12 + " << (base + 8) << "], 0\n";
            }
            return;
         }
         if (type.is_array) {
            int element_size   = type.elem_size(); // array base size
            int base  = gen->m_current_offset; // this array's element 0
            gen->m_vars.push_back(Var { .name = h->ident.text(), .offset = base, 
                                        .type = type });
            gen->m_current_offset += type.byte_size();
            if (h->expr) {
               auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->variant);
               const auto& elems = (*lit)->elements;
               for (size_t i = 0; i < elems.size(); i++) {
                  gen->gen_expr(elems[i]);
                  gen->pop("rax");
                  int off = base + (int)i * element_size;
                  if (element_size == 8) gen->m_output << "   mov QWORD [r12 + " << off  << "], rax\n";
                  else          gen->m_output << "   mov byte [r12 + " << off << "], al\n";
               }
            } else {
               // sized, uninit: zero entire region of mem
               const char* store_op = (element_size == 8) ? "rep stosq" : "rep stosb";
               gen->m_output << "   lea rdi, [r12 + " << base << "]\n"
                              << "   mov rcx, " << type.array_len << "\n"
                              << "   xor rax, rax\n"
                              << "   " << store_op << "\n"; 
            }
            return;
         }

         
         // Scalar
         int off = gen->m_current_offset;
         gen->m_vars.push_back(Var { 
            .name = h->ident.text(), 
            .offset = off, .type = type 
         });
         
         gen->m_current_offset += type.byte_size();
         bool is_char = type.base == DataType::CHAR;
         
         if (h->expr) {
            gen->gen_expr(h->expr);
            gen->pop("rax");
            gen->m_output << (is_char ? gen->store_scalar(off, "al", "byte") :
                                        gen->store_scalar(off, "rax", "QWORD"));
         } else {
            gen->m_output << gen->store_scalar(off, "0", (is_char ? "byte" : "QWORD" ));
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

         // Regular assignment 
         if (auto* id = std::get_if<NodeExprIdent*>(&assign->target->variant)) {
            auto var = gen->get_var((*id)->ident.text());
            if (!var.has_value()) {
               std::cerr << "Undeclared identifier assign: " << assign->ident.text() << std::endl;
               gen->total_fail();
            }
            int off = var.value().offset;
            
            if (var.value().type.base == DataType::STR) {
              
               if (auto* lit = std::get_if<NodeExprStrLit*>(&assign->expr->variant)) {
                  gen->store_string_literal(off, (*lit)->STR_LIT.text());
               }
               else if (auto* rhs_id = std::get_if<NodeExprIdent*>(&assign->expr->variant)) {
                  auto rhs_var = gen->get_var((*rhs_id)->ident.text());
                  if (!rhs_var.has_value()) { 
                     std::cerr << "Undeclared ident: " << (*rhs_id)->ident.text() << std::endl; 
                     gen->total_fail(); 
                  }
               
                  int rhs_off = rhs_var.value().offset;
                  gen->m_output << "   mov rax, QWORD [r12 + " << rhs_off << "]\n"        // load other's ptr
                                << "   mov QWORD [r12 + " << off << "], rax\n"            // store to target's ptr  
                                << "   mov rax, QWORD [r12 + " << (rhs_off + 8) << "]\n"  // load other's len
                                << "   mov QWORD [r12 + " << (off + 8) << "], rax\n";     // store to target's len
               }
               else {
                  std::cerr << "String assignment RHS must be a str lit or str var (for now).\n";
                  gen->total_fail();
               }
            }
            else if (var.value().type.base == DataType::CHAR) {
               gen->gen_expr(assign->expr);
               gen->pop("rax");
               gen->m_output << "   mov byte [r12 + " << off << "], al\n";
            }            
            else {
               gen->gen_expr(assign->expr);
               gen->pop("rax");
               gen->m_output << "   mov QWORD [r12 + " << off << "], rax\n";
            }
         }
         else if (auto* idx = std::get_if<NodeExprIndex*>(&assign->target->variant)) {
            gen->gen_expr(assign->expr);
            gen->pop("rax");
            auto var = gen->get_var((*idx)->ident.text());
            if (!var.has_value()) {
               std::cerr << "Undeclared identifier index assign: " << assign->ident.text() << std::endl;
               gen->total_fail();
            }
            int esz = var.value().type.elem_size();
            // comp index into rbx
            gen->push("rax");             // save value
            gen->gen_expr((*idx)->index); // evaluate index
            gen->pop("rbx");              // rbx = index
            gen->pop("rax");              // resture val to assign

            if (esz == 8)
               gen->m_output << "   mov QWORD [r12 + rbx*8 + " << var.value().offset << "], rax\n";
            else
               gen->m_output << "   mov byte [r12 + rbx + " << var.value().offset << "], al\n";

         }
      }
   
   
      void operator()(const NodeStmtFor* loop) const {
         gen->begin_scope();

         gen->gen_stmt(loop->init);
         std::string strt = gen->make_label();
         std::string ender = gen->make_label();
         gen->m_output << strt << ":\n";
         gen->gen_cond(loop->condition, ender);
         
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
         if (gen->m_curr_ret_type.base == DataType::STR) {
            // str ret ptr rax, len rdx
            if (auto* lit = std::get_if<NodeExprStrLit*>(&_return->expr->variant)) {
               const std::string& str = (*lit)->STR_LIT.text();
               std::string label = gen->add_string(str);
               gen->m_output << "   lea rax, [" << label << "]\n"
                             << "   mov rdx, " << str.size() << "\n";
            }
            else if (auto* id = std::get_if<NodeExprIdent*>(&_return->expr->variant)) {
                  auto var = gen->get_var((*id)->ident.text());
                  int off = var.value().offset;
                  gen->m_output << "   mov rax, QWORD [r12 + " << off << "]\n"
                                << "   mov rdx, QWORD [r12 + " << (off + 8) << "]\n";
            }
            else {
               std::cerr << "Unsupported str return expr.\n";
               gen->total_fail();
            }

         } 
         else if (gen->m_curr_ret_type.base == DataType::CHAR) {
            if (auto* lit = std::get_if<NodeExprCharLit*>(&_return->expr->variant)) {
               char ch = (*lit)->CHAR_LIT.char_val();
               gen->m_output << "   mov al, '";
               if (ch == '\'') gen->m_output << "\\";
               gen->m_output << ch << "'\n";
            }
            else if (auto* id = std::get_if<NodeExprIdent*>(&_return->expr->variant)) {
               auto var = gen->get_var((*id)->ident.text());
               int off = var.value().offset;
               gen->m_output << "   mov al, byte [r12 + " << off << "]\n";
            }
            else {
               std::cerr << "Unsupported char return expr.\n";
               gen->total_fail();
            }
         }
         else {
            // scalar return
            gen->gen_expr(_return->expr);
            gen->pop("rax");
         }
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
         DataType t = p->expr->resolved.base;
         switch (t) {
            case DataType::INT:
               gen->gen_expr(p->expr);
               gen->pop("rax");
               gen->m_output << "   mov rdi, " << (p->nwln ? 1 : 0) << '\n';
               gen->m_output << "   call print_int\n";
               break;
            case DataType::STR: {
               if (!p->expr) {
                  if (p->nwln) {
                     gen->m_output << "   mov byte [print_buf], LF\n"
                                   << "   mov rax, SYS_write\n"
                                   << "   mov rdi, STDOUT\n"
                                   << "   mov rsi, print_buf\n"
                                   << "   mov rdx, 1\n"
                                   << "   syscall\n";
                  }
               }
               else if (auto* lit = std::get_if<NodeExprStrLit*>(&p->expr->variant)) {
                  const std::string& bytes = (*lit)->STR_LIT.text();
                  std::string label = gen->add_string(bytes);

                  gen->m_output << "   mov rax, SYS_write\n"
                                << "   mov rdi, STDOUT\n"
                                << "   mov rsi, " << label << '\n'
                                << "   mov rdx, " << label << "_len\n"
                                << "   syscall\n";
               }
               else if (auto* id = std::get_if<NodeExprIdent*>(&p->expr->variant)) {
                  auto var = gen->get_var((*id)->ident.text());
                  if (!var.has_value()) { std::cerr << "Undeclared identifier: " << (*id)->ident.text() << std::endl; gen->total_fail(); }
                  int off = var.value().offset;
                  gen->m_output << "   mov rsi, QWORD [r12 + " << off << "]\n"
                                << "   mov rdx, QWORD [r12 + " << (off + 8) << "]\n"
                                << "   mov rax, SYS_write\n"
                                << "   mov rdi, STDOUT\n"
                                << "   syscall\n";
               }
               else { /* other str expr not supported yet */}
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
               gen->total_fail();
         }
      }
   };

   StmtVisitor visitor { .gen = this };
   std::visit(visitor, stmt->variant);
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
         if (gen->try_load_simple(cmp->right, "rbx"))
            gen->pop("rax");
         else {
            gen->gen_expr(cmp->right);
            gen->pop("rbx");
            gen->pop("rax");
         }
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
         if (gen->try_load_simple(cmp->right, "rbx"))
            gen->pop("rax");
         else {
            gen->gen_expr(cmp->right);
            gen->pop("rbx");
            gen->pop("rax");
         }
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
   for (const NodeParam& p : func->params)
      frame_size += p.type.byte_size();
   frame_size = (frame_size + 15) & ~15;
   m_uses_frame_base = (frame_size > 0 || !func->params.empty());

   if (func->has_ret_type) m_curr_ret_type.base = Symbols::tok_dt(func->ret_type.type); /** TODO: Fix parsing of function ret type to look for arrays */
   else m_curr_ret_type = TypeInfo {};

   m_output << func->name.text() << ":\n"
            << "   push rbp\n";

   if (m_uses_frame_base) m_output << "   push r12\n";
   m_output << "   mov rbp, rsp\n";
   if (m_uses_frame_base) {
      m_output << "   sub rsp, " << frame_size << "\n"
               << "   mov r12, rsp\n";
   }

   begin_scope();
   static std::string param_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
   int reg_idx = 0;
   
   for (const NodeParam& p : func->params) {
      const TypeInfo& pt = p.type;
      int off = m_current_offset;
      m_vars.push_back(Var {
         .name = p.name.text(),
         .offset = off,
         .type = pt
      });

      int regs_used = param_reg_count(pt);
      if (reg_idx + regs_used > 6) {
         std::cerr << "Too many register args (strs count as 2).\n";
         total_fail();
      }

      if (pt.base == DataType::STR) {
         // 2 regs -> 16-byte slot: ptr at off, len at off + 8
         m_output << "   mov QWORD [r12 + " << off << "], " << param_regs[reg_idx] << "\n"            // ptr
                  << "   mov QWORD [r12 + " << (off + 8) << "], " << param_regs[reg_idx + 1] << "\n"; // len
      } 
      else if (pt.base == DataType::CHAR)
         // byte-width spill, storw in low byte of arg reg
         m_output << "   mov byte [r12 + " << off << "], " << reg_map.at(param_regs[reg_idx])._8_bits.low << "\n";
      else
         // int / bool
         m_output << "   mov QWORD [r12 + " << off << "], " << param_regs[reg_idx] << "\n";
      m_current_offset += pt.byte_size();

      reg_idx += regs_used;
   }
   

   for (auto stmt : func->body->stmts)
      gen_stmt(stmt);
   end_scope();

   emit_epilogue();
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
   for (const NodeFunction* f : m_prog.funcs) {
      TypeInfo ret;
      if(f->has_ret_type) ret.base = Symbols::tok_dt(f->ret_type.type);
      m_funcs[f->name.text()] = ret;
   }

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
}


void ASMGenerator::end_scope() {
   size_t var_before = m_scope_stack.back();
   m_scope_stack.pop_back();
   m_vars.resize(var_before);
   /** NOTE: m_current_offset intentionally not rolled back, no mem reclaiming (yet) */
}


std::optional<ASMGenerator::Var> ASMGenerator::get_var(const std::string& name) {
   for (auto it = m_vars.rbegin(); it != m_vars.rend(); ++it)
      if (it->name == name) return *it;
   return {};
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
         else if constexpr (std::is_same_v<T, NodeStmtFor>) {
            if (s->init) bytes += compute_frame_size(std::vector<NodeStmt*>{ s->init });
            bytes += compute_frame_size(s->body->stmts);
         }
         else if constexpr (std::is_same_v<T, NodeStmtIf>) {
            bytes += compute_frame_size(s->body->stmts);
            if (s->else_body) bytes += compute_frame_size(s->else_body->stmts);
         }
         else if constexpr (std::is_same_v<T, NodeScopeBlock>)
            bytes += compute_frame_size(s->stmts);
      }, stmt->variant);
   }
   return bytes;
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
   else if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->variant)) {
      const auto& elems = (*lit)->elements;
      if (elems.empty()) { std::cerr << "Elements are empty.\n"; total_fail(); }
      DataType et = elems[0]->resolved.base;
      for (size_t i = 1; i < elems.size(); i++) {
         if (elems[i]->resolved.base != et) {
            std::cerr << "Array literal has mismatched element types." << std::endl;
            total_fail();
         }
      }

      info.base = et;
      info.is_array = true;
      info.array_len = elems.size();
   }
   else info.base = h->expr->resolved.base; // scalar inference

   h->resolved = info;
   h->is_resolved = true;
   return info;
}


bool ASMGenerator::try_load_simple(const NodeExpr* e, const std::string& reg) {
   if (auto* lit = std::get_if<NodeExprIntLit*>(&e->variant)) {
      m_output << "   mov " << reg << ", " << (*lit)->INT_LIT.int_val() << "\n";
      return true;
   }
   if (auto* id = std::get_if<NodeExprIdent*>(&e->variant)) {
      auto var = get_var((*id)->ident.text());
      if (var.has_value()) {
         m_output << "   mov " << reg << ", QWORD [r12 + " << var.value().offset << "]\n";
         return true;
      }
   }
   return false; // not simple -> caller falls back on gen_expr
}


void ASMGenerator::emit_epilogue() {
   if (m_uses_frame_base) {
      m_output << "   mov rsp, rbp\n"
               << "   pop r12\n"
               << "   pop rbp\n"
               << "   ret\n";
   } else {
      m_output << "   pop rbp\n"
               << "   ret\n";
   }
}


void ASMGenerator::total_fail() {
   std::fstream file("failed.asm", std::ios::out);
   file << m_output.str();
   file.close();
   exit(EXIT_FAILURE);
}


void ASMGenerator::store_string_literal(int off, const std::string& str) {
   std::string label = add_string(str);
   m_output << "   lea rax, [" << label << "]\n"
            << "   mov QWORD [r12 + " << off << "], rax\n"
            << "   mov QWORD [r12 + " << (off + 8) << "], " << str.size() << "\n";
}


int ASMGenerator::param_reg_count(const TypeInfo& t) {
   if (t.base == DataType::STR) return 2;
   return 1;
   // later for struct: ceil(size/8) for <= 16 bytes, else push to stack.
}


std::string ASMGenerator::store_scalar(int offset, const std::string& reg = "rax", const std::string& op = "QWORD") {
   std::stringstream output;
   output << "   mov " << op << " [r12 + " << offset << "], " << reg << "\n";
   return output.str();
}


std::string ASMGenerator::load_scalar(int offset, const std::string& mv = "mov", 
                                                  const std::string& reg = "rax", 
                                                  const std::string& op = "QWORD") {
   std::stringstream output;
   output << "   " << mv << " " << reg << ", " << op << " [r12 + " << offset << "]\n";
   return output.str(); 
}