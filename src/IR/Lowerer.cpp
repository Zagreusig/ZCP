#include "Lowerer.h"
#include "Core/SymbolTable.h"

IRModule Lowerer::lower(const NodeProg& prog) {
   IRModule mod;
   for (const NodeFunction* function : prog.functions) 
      mod.functions.push_back(lower_function(function));
   return mod;
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
      IRType param_type = Symbols::ir_type_of(param.type);
      VReg param_vreg = fresh(param_type);
      out.params.push_back(param_vreg);

      VReg addr = fresh(IRType::Ptr);
      emit_alloca(addr, param.type.byte_size());
      // { IRInstruction a(IROp::Alloca, addr); a.imm = param.type.byte_size(); emit(a); }
      emit_store(addr, param_vreg);
      // { IRInstruction s(IROp::Store); 
      //   s.operands.push_back(IROperand::make_reg(addr));
      //   s.operands.push_back(IROperand::make_reg(param_vreg));
      //   emit(s); }
      declare_var(param.name.text(), addr);
   }

   lower_scope(function->body);
   
   // implicit return if the last block fell through
   if (!m_block->terminated()) {
      IRInstruction ret(IROp::Ret);
      if (out.ret_type != IRType::Void) {
         VReg zero = fresh(IRType::I64);
         emit_const(zero, 0);
         // IRInstruction c(IROp::Const, zero);
         // c.operands.push_back(IROperand::make_const(0));
         // emit(c);
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
            emit_store(addr, val);
            // IRInstruction st(IROp::Store);
            // st.operands.push_back(IROperand::make_reg(addr));
            // st.operands.push_back(IROperand::make_reg(val));
            // emit(st);
         }
         declare_var(s->ident.text(), addr);
         (void)t;
      }
      else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
         // target is an lvalue expr: ident (simple) or index (element).
         VReg val = lower_expr(s->expr);
         VReg addr = lower_lvalue_address(s->target);
         emit_store(addr, val);
         // IRInstruction st(IROp::Store);
         // st.operands.push_back(IROperand::make_reg(addr));
         // st.operands.push_back(IROperand::make_reg(val));
         // emit(st);
      }
      else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
         IRInstruction r(IROp::Ret);
         if (s->expr) {
            VReg val = lower_expr(s->expr);
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
      // NodeStmtExit, NodeStmtPrint: lowered via calls to runtime - deferred.
   }, stmt->variant);
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
         // IRInstruction cmp(Symbols::cmp_to_ir(c->operation), res);
         // cmp.operands.push_back(IROperand::make_reg(l));
         // cmp.operands.push_back(IROperand::make_reg(r));
         // emit(cmp);
         emit_condbr(res, true_id, false_id);
         // IRInstruction cbr(IROp::CondBr);
         // cbr.operands.push_back(IROperand::make_reg(res));
         // cbr.operands.push_back(IROperand::make_block(true_id));
         // cbr.operands.push_back(IROperand::make_block(false_id));
         // emit(cbr);
      }
      else if constexpr (std::is_same_v<T, NodeLogicCondition>) {
         lower_logop(m_block->id, c->operation, c->left, c->right, true_id, false_id);
         // if (c->operation == CmpExprType::AND) {
            
            // int pred = m_block->id, rhs = make_block("and.rhs");
            // switch_to(pred);
            // lower_condition(c->left, rhs, false_id); // left false -> whole false
            // switch_to(rhs);
            // lower_condition(c->right, true_id, false_id);
         // }
         // else { // OR 
         //    int pred = m_block->id, rhs = make_block("or.rhs");
         //    switch_to(pred);
         //    lower_condition(c->left, true_id, rhs);
         //    switch_to(rhs);
         //    lower_condition(c->right, true_id, false_id);
         // }
      }
   }, cond->variant);
}


VReg Lowerer::lower_lvalue_address(const NodeExpr* target) {
   return std::visit([&](auto* e) ->VReg {
      using T = std::decay_t<decltype(*e)>;
      if constexpr (std::is_same_v<T, NodeExprIdent>)
         return lookup_var(e->ident.text());
      else if constexpr (std::is_same_v<T, NodeExprIndex>) {
         VReg base = lookup_var(e->ident.text()); // array base address
         VReg idx  = lower_expr(e->index);
         VReg elem = fresh(IRType::Ptr);
         emit_GetElemPtr(base, idx, elem, target->resolved.element_size()); /** TODO: get size from array's elem type. */
         // IRInstruction gep(IROp::GetElemPtr, elem);
         // gep.operands.push_back(IROperand::make_reg(base));
         // gep.operands.push_back(IROperand::make_reg(idx));
         // gep.imm = 8; 
         // emit(gep);
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
         // IRInstruction i(IROp::Const, dest);
         // i.operands.push_back(IROperand::make_const(e->INT_LIT.int_val()));
         // emit(i);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprCharLit>) {
         VReg dest = fresh(IRType::I8);
         emit_const(dest, (int64_t)e->CHAR_LIT.char_val());
         // IRInstruction i(IROp::Const, dest);
         // i.operands.push_back(IROperand::make_const((int64_t)e->CHAR_LIT.char_val()));
         // emit(i);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprBoolLit>) {
         VReg dest = fresh(IRType::I8);
         emit_const(dest, e->BOOL_LIT.type == TokenType::TRUE ? 1 : 0);
         // IRInstruction i(IROp::Const, dest);
         // int64_t v = (e->BOOL_LIT.type == TokenType::TRUE) ? 1 : 0;
         // i.operands.push_back(IROperand::make_const(v));
         // emit(i);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprIdent>) {
         VReg addr = lookup_var(e->ident.text());
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_one_reg(IROp::Load, dest, addr);
         // IRInstruction i(IROp::Load, dest);
         // i.operands.push_back(IROperand::make_reg(addr));
         // emit(i);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprIndex>) {
         VReg base = lookup_var(e->ident.text()),
              idx  = lower_expr(e->index),
              elem = fresh(IRType::Ptr);
         emit_GetElemPtr(base, idx, elem, expr->resolved.element_size());
         // { IRInstruction gep(IROp::GetElemPtr, elem);
         //   gep.operands.push_back(IROperand::make_reg(base));
         //   gep.operands.push_back(IROperand::make_reg(idx));
         //   gep.imm = 8; /** TODO: get from aray */
         //   emit(gep); }
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_one_reg(IROp::Load, dest, elem);
         // IRInstruction ld(IROp::Load, dest);
         // ld.operands.push_back(IROperand::make_reg(elem));
         // emit(ld);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeBinExpr>) {
         VReg l = lower_expr(e->left), r = lower_expr(e->right),
              dest = fresh(Symbols::ir_type_of(expr->resolved));
         emit_binop(Symbols::binop_to_ir(e->operation), dest, l, r);
         // IRInstruction i(Symbols::binop_to_ir(e->operation), dest);
         // i.operands.push_back(IROperand::make_reg(l));
         // i.operands.push_back(IROperand::make_reg(r));
         // emit(i);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprCall>) {
         IRInstruction call(IROp::Call);
         VReg dest = fresh(Symbols::ir_type_of(expr->resolved));
         call.dest = dest;
         call.operands.push_back(IROperand::make_symbol(e->name.text()));
         for (const NodeExpr* arg : e->args)
            call.operands.push_back(IROperand::make_reg(lower_expr(arg)));
         emit(call);
         return dest;
      }
      else if constexpr (std::is_same_v<T, NodeExprIncDec>) {
         // x++ / ++x : load, add / sub 1, store, return appropriate values
         VReg addr = lookup_var(e->ident.text());
         VReg old  = fresh(IRType::I64);
         emit_one_reg(IROp::Load, old, addr);
         // { IRInstruction ld(IROp::Load, old); ld.operands.push_back(IROperand::make_reg(addr)); emit(ld); }
         VReg one = fresh(IRType::I64);
         emit_const(one, 1);
         // { IRInstruction c(IROp::Const, one); c.operands.push_back(IROperand::make_const(1)); emit(c); }
         VReg nv = fresh(IRType::I64);
         emit_binop(e->is_increment ? IROp::Add : IROp::Sub, nv, old, one);
         // { IRInstruction op(e->is_increment ? IROp::Add : IROp::Sub, nv);
         //   op.operands.push_back(IROperand::make_reg(old));
         //   op.operands.push_back(IROperand::make_reg(one)); emit(op); }
         emit_store(addr, nv);
         // { IRInstruction st(IROp::Store);
         //   st.operands.push_back(IROperand::make_reg(addr));
         //   st.operands.push_back(IROperand::make_reg(nv)); emit(st); }
         return e->is_prefix ? nv : old; // prefix returns new, postfix returns old
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


void Lowerer::lower_logop(int pred, CmpExprType op, NodeCondition* left, NodeCondition* right, int true_id, int false_id) {
   int rhs = make_block(op == CmpExprType::AND ? "and.rhs" : "or.rhs");
   switch_to(pred);
   if (op == CmpExprType::AND) lower_condition(left, rhs, false_id); // left true -> eval right; left false -> whole false
   else                        lower_condition(left, true_id, rhs);  // left true -> while true; left false -> eval right
   switch_to(rhs);
   lower_condition(right, true_id, false_id);
}