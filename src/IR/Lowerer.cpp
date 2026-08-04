#include "Lowerer.h"

#include <type_traits>
#include <variant>

#include "Core/TypeConversions.h"
#include "Core/IRDefs.h"
#include "Core/Nodes.h"
#include "Core/TokenTable.h"
#include "Core/Tokens.h"
#include "utils/msc.h"

IRModule Lowerer::lower(const NodeProg& prog) {
   for (const NodeFunction* function : prog.functions) 
      m_module.functions.push_back(lower_function(function));
   return m_module;
}

IRFunction Lowerer::lower_function(const NodeFunction* function) {
   IRFunction out;
   out.name = function->name.text();
   out.ret_type = function->has_ret_type ? Symbols::ir_type_of(function->ret_type.type) : IRType::Void;

   // stub
   if (!function->body) { out.is_declaration = true; return out; }

   m_function = &out;
   m_scopes.clear();
   push_scope();

   IRBasicBlock& entry = out.new_block("entry");
   m_block = &entry;

   // parameters: each -> a VReg, stored into an alloca so the body reads
   // it uniformly via load. Param type vary (NodeParam::type)/
   for (const NodeParam& param : function->params) {
      if (param.type.base == DataType::STR) {
         // STR params take 2 incoming regs (ptr, len), matching the old
         // codegen's ABI - see NodeExprCall below for the caller side.
         VReg ptr_vreg = fresh(IRType::Ptr), len_vreg = fresh(IRType::I64);
         out.params.push_back(ptr_vreg);
         out.params.push_back(len_vreg);

         VReg addr = fresh(IRType::Ptr);
         emit_alloca(addr, 16);
         emit_store(addr, ptr_vreg);
         VReg len_addr = fresh(IRType::Ptr);
         emit_GetElemPtr(len_addr, addr, 1, 8);
         emit_store(len_addr, len_vreg);
         declare_var(param.name.text(), addr, true);
         continue;
      }

      IRType param_type = Symbols::ir_type_of(param.type);
      VReg param_vreg = fresh(param_type);
      out.params.push_back(param_vreg);

      VReg addr = fresh(IRType::Ptr);
      emit_alloca(addr, param.type.byte_size());
      emit_store(addr, param_vreg);
      declare_var(param.name.text(), addr);
   }

   lower_scope(function->body);
   
   // implicit return if the last block fell through
   if (!m_block->terminated()) {
      IRInstruction ret(IROp::Ret);
      if (out.ret_type != IRType::Void) {
         VReg zero = fresh(IRType::I64);
         emit_const(zero, 0);
         ret.operands.push_back(IROperand::make_reg(zero));
      }
      emit(ret);
   }

   pop_scope();
   m_function = nullptr; m_block = nullptr;
   return out;
}


void Lowerer::lower_scope(const NodeScopeBlock* block) {
   push_scope();
   for (const NodeStmt* stmt : block->stmts) {
      lower_stmt(stmt);
      if (m_block->terminated()) break; // rest unreachable
   }
   pop_scope();
}


void Lowerer::lower_stmt(const NodeStmt* stmt) {
   std::visit([&](auto* s) {
      using T = std::decay_t<decltype(*s)>;

      if constexpr (std::is_same_v<T, NodeStmtHave>) {
         IRType t = Symbols::ir_type_of(s->resolved);
         VReg addr = fresh(IRType::Ptr);
         { IRInstruction a(IROp::Alloca, addr); a.imm = s->resolved.byte_size(); emit(a); }
         if (s->expr) {
            VReg val = lower_expr(s->expr);
            if (s->resolved.base == DataType::STR) emit_copy_str(addr, val);
            else emit_store(addr, val);
         }
         declare_var(s->ident.text(), addr, s->resolved.base == DataType::STR);
         (void)t;
      }
      else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
         // target is an lvalue expr: ident (simple) or index (element).
         VReg val = lower_expr(s->expr);
         VReg addr = lower_lvalue_address(s->target);
         if (s->target->resolved.base == DataType::STR) emit_copy_str(addr, val);
         else emit_store(addr, val);
      }
      else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
         IRInstruction r(IROp::Ret);
         if (s->expr) {
            VReg val = lower_expr(s->expr);
            if (s->expr->resolved.base == DataType::STR) {
               VReg ptr_val = fresh(IRType::Ptr);
               emit_load(ptr_val, val);
               VReg len_addr = fresh(IRType::Ptr);
               emit_GetElemPtr(len_addr, val, 1, 8);
               VReg len_val = fresh(IRType::I64);
               emit_load(len_val, len_addr);
               r.operands.push_back(IROperand::make_reg(ptr_val));
               r.operands.push_back(IROperand::make_reg(len_val));
            }
            else
               r.operands.push_back(IROperand::make_reg(val));
         }
         emit(r);
      }
      else if constexpr (std::is_same_v<T, NodeStmtIf>)
         lower_if(s);
      else if constexpr (std::is_same_v<T, NodeStmtWhile>)
         lower_while(s);
      else if constexpr (std::is_same_v<T, NodeStmtFor>)
         lower_for(s);
      else if constexpr (std::is_same_v<T, NodeStmtScope>)
         lower_scope(s->scope);
      else if constexpr (std::is_same_v<T, NodeScopeBlock>)
         lower_scope(s);
      else if constexpr (std::is_same_v<T, NodeStmtExpr>)
         lower_expr(s->expr); // side effects (call, ind/dec); result discarded....
      else if constexpr (std::is_same_v<T, NodeStmtPrint>)
         lower_print(s);
      else if constexpr (std::is_same_v<T, NodeStmtExit>)
         lower_exit(s);
   }, stmt->variant);
}


void Lowerer::lower_print(const NodeStmtPrint* stmt) {
   int64_t nl = stmt->nwln ? 1 : 0; // newline flag

   if (!stmt->expr) {
      IRInstruction call(IROp::Call);
      call.operands.push_back(IROperand::make_symbol("print_nl"));
      emit(call);
      return;
   }

   TypeInfo tinfo = stmt->expr->resolved;
   VReg val = lower_expr(stmt->expr);        // ptr in case of STR
   IRInstruction call(IROp::Call);

   if (tinfo.base == DataType::STR) {
      VReg ptr = fresh(IRType::Ptr);

      // ptr = load [val]
      emit_load(ptr, val);

      // len = load [val + 8]
      VReg len_addr = fresh(IRType::Ptr);
      emit_GetElemPtr(len_addr, val, 1, 8);

      VReg len = fresh(IRType::I64);
      emit_load(len, len_addr);
      call.operands.push_back(IROperand::make_symbol("print_str"));
      call.operands.push_back(IROperand::make_reg(ptr));    // rdi
      call.operands.push_back(IROperand::make_reg(len));    // rsi
      call.operands.push_back(IROperand::make_const(nl));   // rdx (low byte = dl)
      emit(call); return; // leave early so the rest doesn't happen :D
   }
   else if (tinfo.base == DataType::CHAR) 
      call.operands.push_back(IROperand::make_symbol("print_char"));
   else // int / bool
      call.operands.push_back(IROperand::make_symbol("print_int"));

   call.operands.push_back(IROperand::make_reg(val));               // in print_char -> rdi/dil
   call.operands.push_back(IROperand::make_const(nl));              // in print_char -> rsi/sil
   emit(call);
}


void Lowerer::lower_exit(const NodeStmtExit* stmt) {
   VReg code = lower_expr(stmt->expr);
   IRInstruction call(IROp::Call);
   call.operands.push_back(IROperand::make_symbol("sys_exit"));
   call.operands.push_back(IROperand::make_reg(code));
   emit(call);

   IRInstruction _exit(IROp::Exit);
   emit(_exit);
}


void Lowerer::lower_if(const NodeStmtIf* stmt) {
   int pred = m_block->id, then_id = make_block("if.then"),
       else_id = stmt->else_body ? make_block("if.else") : -1,
       end_id = make_block("if.end"),
       false_target = stmt->else_body ? else_id : end_id;
   
   switch_to(pred);
   lower_condition(stmt->condition, then_id, false_target);

   switch_to(then_id);
   lower_scope(stmt->body);
   branch_to(end_id);

   if (stmt->else_body) {
      switch_to(else_id);
      lower_scope(stmt->else_body);
      branch_to(end_id);
   }
   switch_to(end_id);
}


void Lowerer::lower_while(const NodeStmtWhile* stmt) {
   int pred = m_block->id, cond_id = make_block("loop.cond"),
       body_id = make_block("loop.body"), end_id = make_block("loop.end");

   switch_to(pred);
   branch_to(cond_id);

   switch_to(cond_id);
   lower_condition(stmt->condition, body_id, end_id);

   switch_to(body_id);
   lower_scope(stmt->body);
   branch_to(cond_id);

   switch_to(end_id);
}


void Lowerer::lower_for(const NodeStmtFor* stmt) {
   // for (init; cont; incr) body
   push_scope();
   if (stmt->init) lower_stmt(stmt->init);

   int pred = m_block->id, cond_id = make_block("for.cond"),
       body_id = make_block("for.body"), end_id = make_block("for.end");

   switch_to(pred);
   branch_to(cond_id);

   switch_to(cond_id);
   if (stmt->condition) lower_condition(stmt->condition, body_id, end_id);
   else branch_to(body_id); // no condition -> infinite (until break) <- impossible.

   switch_to(body_id);
   lower_scope(stmt->body);
   if (stmt->increment) lower_stmt(stmt->increment);
   branch_to(cond_id);

   switch_to(end_id);
   pop_scope();
}


void Lowerer::lower_condition(const NodeCondition* cond, int true_id, int false_id) {
   std::visit([&](auto* c) {
      using T = std::decay_t<decltype(*c)>;
      
      if constexpr (std::is_same_v<T, NodeCmpCondition>) {
         VReg l = lower_expr(c->left), r = lower_expr(c->right);
         VReg res = fresh(IRType::I64);
         emit_binop(Symbols::cmp_to_ir(c->operation), res, l, r);
         emit_condbr(res, true_id, false_id);
      }
      else if constexpr (std::is_same_v<T, NodeLogicCondition>) 
         lower_logop(m_block->id, c->operation, c->left, c->right, true_id, false_id);
      
   }, cond->variant);
}


VReg Lowerer::lower_lvalue_address(const NodeExpr* target) {
   return std::visit([&](auto* e) ->VReg {
      using T = std::decay_t<decltype(*e)>;
      if constexpr (std::is_same_v<T, NodeExprIdent>)
         return lookup_var(e->ident.text());
      else if constexpr (std::is_same_v<T, NodeExprIndex>) {
         VReg base = lookup_var(e->ident.text()); // array base address
         if (lookup_is_str(e->ident.text())) {
            // analyzer accepts `s[i] = ...` for STR (it only type-checks the
            // element type), so mirror the read side: deref the ptr field
            // before indexing into the actual char data.
            VReg ptr_val = fresh(IRType::Ptr);
            emit_load(ptr_val, base);
            VReg idx = lower_expr(e->index), elem = fresh(IRType::Ptr);
            emit_GetElemPtr(ptr_val, idx, elem, 1);
            return elem;
         }
         VReg idx  = lower_expr(e->index);
         VReg elem = fresh(IRType::Ptr);
         emit_GetElemPtr(base, idx, elem, target->resolved.element_size());
         return elem;
      }
      else
         return VReg{}; // not an lvalue (analyzer should reject)
   }, target->variant);
}


VReg Lowerer::lower_expr(const NodeExpr* expr) {
   return std::visit([&](auto* e) -> VReg {
      using T = std::decay_t<decltype(*e)>;

      if constexpr (std::is_same_v<T, NodeExprIntLit>) {
         VReg dest = fresh(IRType::I64);
         emit_const(dest, e->INT_LIT.int_val());
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprCharLit>) {
         VReg dest = fresh(IRType::I8);
         emit_const(dest, (int64_t)e->CHAR_LIT.char_val());
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprBoolLit>) {
         VReg dest = fresh(IRType::I8);
         emit_const(dest, e->BOOL_LIT.type == TokenType::TRUE ? 1 : 0);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprStrLit>) {
         auto [label, len] = intern_string(e->STR_LIT.text());

         VReg fp = fresh(IRType::Ptr);
         emit_alloca(fp, 16);

         VReg ptr = fresh(IRType::Ptr);
         emit_symbol(IROp::GlobalAddr, ptr, label);
         emit_store(fp, ptr);

         VReg len_addr = fresh(IRType::Ptr);
         emit_GetElemPtr(len_addr, fp, 1, 8);
         
         VReg len_val = fresh(IRType::I64);
         emit_const(len_val, len);
         emit_store(len_addr, len_val);
         return fp;
      }
      else if constexpr (std::is_same_v<T, NodeExprIdent>) {
         VReg addr = lookup_var(e->ident.text());
         if (expr->resolved.base == DataType::STR) return addr; // STR VReg = struct address
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_one_reg(IROp::Load, dest, addr);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprIndex>) {
         VReg base = lookup_var(e->ident.text());
         if (lookup_is_str(e->ident.text())) {
            // base is a STR struct's address: deref its ptr field first,
            // then index into the actual char data it points at.
            VReg ptr_val = fresh(IRType::Ptr);
            emit_load(ptr_val, base);
            VReg idx = lower_expr(e->index), elem = fresh(IRType::Ptr);
            emit_GetElemPtr(ptr_val, idx, elem, 1);
            VReg dest = fresh(IRType::I8);
            emit_one_reg(IROp::Load, dest, elem);
            return dest;
         }
         VReg idx  = lower_expr(e->index),
              elem = fresh(IRType::Ptr);
         emit_GetElemPtr(base, idx, elem, expr->resolved.element_size());
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_one_reg(IROp::Load, dest, elem);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeBinExpr>) {
         VReg l = lower_expr(e->left), r = lower_expr(e->right),
              dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_binop(Symbols::binop_to_ir(e->operation), dest, l, r);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprCall>) {
         IRInstruction call(IROp::Call);
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         call.dest = dest;
         call.operands.push_back(IROperand::make_symbol(e->name.text()));
         for (const NodeExpr* arg : e->args) {
            if (arg->resolved.base == DataType::STR) {
               VReg struct_addr = lower_expr(arg);
               VReg ptr_val = fresh(IRType::Ptr);
               emit_load(ptr_val, struct_addr);
               VReg len_addr = fresh(IRType::Ptr);
               emit_GetElemPtr(len_addr, struct_addr, 1, 8);
               VReg len_val = fresh(IRType::I64);
               emit_load(len_val, len_addr);
               call.operands.push_back(IROperand::make_reg(ptr_val));
               call.operands.push_back(IROperand::make_reg(len_val));
            }
            else
               call.operands.push_back(IROperand::make_reg(lower_expr(arg)));
         }
         emit(call);

         if (expr->resolved.base != DataType::STR) return dest;

         // STR return: rax (already captured as `dest`) is the ptr half;
         // rdx (the len half) has nowhere to land on this instruction (one
         // dest per IRInstruction) so CallResult - which MUST immediately
         // follow this Call - captures it separately. Then repackage both
         // into a fresh 16-byte struct, same shape as NodeExprStrLit, so a
         // STR VReg keeps meaning "address of the {ptr,len} struct".
         VReg len_reg = fresh(IRType::I64);
         emit(IRInstruction(IROp::CallResult, len_reg));

         VReg struct_addr = fresh(IRType::Ptr);
         emit_alloca(struct_addr, 16);
         emit_store(struct_addr, dest);
         VReg len_addr = fresh(IRType::Ptr);
         emit_GetElemPtr(len_addr, struct_addr, 1, 8);
         emit_store(len_addr, len_reg);
         return struct_addr;
      }
      else if constexpr (std::is_same_v<T, NodeExprIncDec>) {
         // x++ / ++x : load, add / sub 1, store, return appropriate values
         VReg addr = lookup_var(e->ident.text());
         VReg old  = fresh(IRType::I64);
         emit_one_reg(IROp::Load, old, addr);
         VReg one = fresh(IRType::I64);
         emit_const(one, 1);
         VReg nv = fresh(IRType::I64);
         emit_binop(e->is_increment ? IROp::Add : IROp::Sub, nv, old, one);
         emit_store(addr, nv);
         return e->is_prefix ? nv : old; // prefix returns new, postfix returns old
      }
      else if constexpr (std::is_same_v<T, NodeExprRead>) {
         const char* routine = nullptr;
         IRType result_type = IRType::I64;
         switch (e->kind) {
            case DataType::INT:   routine = "read_int";   result_type = IRType::I64; break;
            case DataType::CHAR:  routine = "read_char";  result_type = IRType::I8;  break;
            case DataType::STR:   routine = "read_str";   result_type = IRType::Ptr; break;
            case DataType::FLOAT: routine = "read_float"; result_type = IRType::I64; break;
            default: break;
         }

         VReg dest = fresh(result_type);
         IRInstruction call(IROp::Call, dest);
         call.operands.push_back(IROperand::make_symbol(routine));
         emit(call);
         return dest;
      }
      else
         return VReg{};
      // NodeExprStrLit, NodeExprArrayLit, NodeExprRead: deferred
      // (strings -> globals + ptr, reads -> runtime calls).
   }, expr->variant);
}


void Lowerer::emit_binop(IROp op, VReg dest, VReg addr1, VReg addr2) {
   IRInstruction instruction(op, dest);
   instruction.operands.push_back(IROperand::make_reg(addr1));
   instruction.operands.push_back(IROperand::make_reg(addr2));
   emit(instruction);
}


void Lowerer::emit_store(VReg addr1, VReg addr2) {
   IRInstruction instruction(IROp::Store);
   instruction.operands.push_back(IROperand::make_reg(addr1));
   instruction.operands.push_back(IROperand::make_reg(addr2));
   emit(instruction);
}


void Lowerer::emit_one_reg(IROp op, VReg dest, VReg reg) {
   IRInstruction instruction(op, dest);
   instruction.operands.push_back(IROperand::make_reg(reg));
   emit(instruction);
}


void Lowerer::emit_const(VReg dest, int64_t val) {
   IRInstruction instruction(IROp::Const, dest);
   instruction.operands.push_back(IROperand::make_const(val));
   emit(instruction);
}


void Lowerer::emit_alloca(VReg dest, int size) {
   IRInstruction instruction(IROp::Alloca, dest);
   instruction.imm = size;
   emit(instruction);
}


void Lowerer::emit_load(VReg dest, VReg origin) {
   IRInstruction instruction(IROp::Load, dest);
   instruction.operands.push_back(IROperand::make_reg(origin));
   emit(instruction);
}


void Lowerer::emit_condbr(VReg res, int true_id, int false_id) {
   IRInstruction cbr(IROp::CondBr);
   cbr.operands.push_back(IROperand::make_reg(res));
   cbr.operands.push_back(IROperand::make_block(true_id));
   cbr.operands.push_back(IROperand::make_block(false_id));
   emit(cbr);
}


void Lowerer::emit_GetElemPtr(VReg base, VReg idx, VReg elem, int size) {
   IRInstruction gep(IROp::GetElemPtr, elem);
   gep.operands.push_back(IROperand::make_reg(base));
   gep.operands.push_back(IROperand::make_reg(idx));
   gep.imm = size;
   emit(gep);
}


void Lowerer::emit_GetElemPtr(VReg base, VReg ptr, int len, int size) {
   IRInstruction gep(IROp::GetElemPtr, base);
   gep.operands.push_back(IROperand::make_reg(ptr));
   gep.operands.push_back(IROperand::make_const(len));
   gep.imm = size;
   emit(gep);
}


void Lowerer::emit_copy_str(VReg dest_addr, VReg src_addr) {
   // ptr field lives at offset 0, so the struct address doubles as its address.
   VReg ptr_val = fresh(IRType::Ptr);
   emit_load(ptr_val, src_addr);
   emit_store(dest_addr, ptr_val);

   VReg src_len_addr = fresh(IRType::Ptr), dest_len_addr = fresh(IRType::Ptr);
   emit_GetElemPtr(src_len_addr, src_addr, 1, 8);
   emit_GetElemPtr(dest_len_addr, dest_addr, 1, 8);

   VReg len_val = fresh(IRType::I64);
   emit_load(len_val, src_len_addr);
   emit_store(dest_len_addr, len_val);
}


void Lowerer::emit_symbol(IROp op, VReg dest, std::string& symbol) {
   IRInstruction sym(op, dest);
   sym.operands.push_back(IROperand::make_symbol(symbol));
   emit(sym);
}


void Lowerer::lower_logop(int pred, LogicOp op, NodeCondition* left, NodeCondition* right, int true_id, int false_id) {
   int rhs = make_block(op == LogicOp::AND ? "and.rhs" : "or.rhs");
   switch_to(pred);
   if (op == LogicOp::AND) lower_condition(left, rhs, false_id); // left true -> eval right; left false -> whole false
   else                        lower_condition(left, true_id, rhs);  // left true -> while true; left false -> eval right
   switch_to(rhs);
   lower_condition(right, true_id, false_id);
}


std::pair<std::string, int64_t> Lowerer::intern_string(const std::string& text) {
   // dedup: if we've seen this exact string, reuse its label
   auto it = m_string_labels.find(text);
   if (it != m_string_labels.end())
      return { it->second, I64(text.size()) };

   std::string label = "str_" + std::to_string(m_module.globals.size());
   IRGlobal global;
   global.label = label;
   global.bytes.assign(text.begin(), text.end());
   m_module.globals.push_back(std::move(global));
   return { label, I64(text.size()) };
}