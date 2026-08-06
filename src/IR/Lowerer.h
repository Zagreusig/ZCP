#ifndef LOWERER_H
#define LOWERER_H

#include <stdint.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <utility>

#include "../Core/IRDefs.h"
#include "../Core/Nodes.h"

enum class LogicOp;
struct NodeCondition;
struct NodeExpr;
struct NodeFunction;
struct NodeProg;
struct NodeScopeBlock;
struct NodeStmt;
struct NodeStmtFor;
struct NodeStmtIf;
struct NodeStmtWhile;
struct NodeStmtExit;
struct NodeStmtPrint;

// ===========================================================================
// Lowerer - Walks the AST and produces three-address IR.
// Core patterns:
//   lower_expr(expr) -> VReg : emits instructions computing expr, returns the
//                              VReg holding the result. Composes: lowering
//                              a + b lowers a and b (two VRegs), emits Add, 
//                              returns its dest.
//   lower_stmt(stmt) -> void : emits instructions for the statement's effect.
//
// Variables live in memory: 'have x' emits an Alloca (address VReg), recorded
// in m_var_addrs[name]. Reading x -> Load from that address; assigning ->
// Store. This alloca / load / store shape will help with SSA construction!
//
// The lowerer tracks a "current block" (m_block) that instructions append to;
// control-flow constructs (added later) create blocks and repoint m_block.
// ===========================================================================


class Lowerer {
public:
   IRModule lower(const NodeProg& prog);
private:
   IRFunction*   m_function = nullptr;
   IRBasicBlock* m_block = nullptr;   

   // scope stack: name -> address VReg (the variable's alloca). Innermost last.
   std::vector<std::unordered_map<std::string, VReg>> m_scopes;

   // parallel scope stack (pushed/popped in lockstep with m_scopes) tracking
   // which names are STR-typed. Kept separate rather than folded into
   // m_scopes's value type so lookup_var and its many call sites stay
   // untouched. Needed because NodeExprIndex must tell a STR base ("s[i]")
   // apart from a char-array base ("arr[i]") - both produce a CHAR element
   // with element_size 1, but only STR's data lives behind an extra pointer
   // indirection, and nothing else (not resolved(), not a symbol table -
   // there isn't one) carries that distinction by the time we're lowering.
   std::vector<std::unordered_map<std::string, bool>> m_str_vars;
   
   // string lit map
   std::unordered_map<std::string, std::string> m_string_labels;

   IRModule m_module;

   // ----- general helpers --------------------------------------------------
   void emit_binop(IROp op, VReg dest, VReg addr1, VReg addr2);
   void emit_store(VReg addr1, VReg addr2);
   void emit_one_reg(IROp op, VReg dest, VReg reg);
   void emit_const(VReg dest, int64_t val);
   void emit_condbr(VReg res, int true_id, int false_id);
   void emit_alloca(VReg dest, int size);
   void emit_load(VReg dest, VReg origin);
   void emit_GetElemPtr(VReg base, VReg idx, VReg elem, int size);
   void emit_GetElemPtr(VReg base, VReg ptr, int len, int size);
   void emit_copy_str(VReg dest_addr, VReg src_addr);
   void emit_symbol(IROp op, VReg dest, std::string& symbol);
   void lower_logop(int pred, LogicOp op, NodeCondition* left, NodeCondition* right, int true_id, int false_id);
   

   // ========================================================================
   // Scope + Variable helpers
   // ========================================================================
   void push_scope() { m_scopes.emplace_back(); m_str_vars.emplace_back(); }
   void pop_scope()  { m_scopes.pop_back(); m_str_vars.pop_back(); }

   void declare_var(const std::string& name, VReg addr, bool is_str = false) {
      m_scopes.back()[name] = addr;
      if (is_str) m_str_vars.back()[name] = true;
   }
   VReg lookup_var(const std::string& name) const {
      for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
         auto found = it->find(name);
         if (found != it->end()) return found->second;
      }
      return VReg{}; // invalid -> undeclared (analyzer should have caught this!)
   }
   bool lookup_is_str(const std::string& name) const {
      for (auto it = m_str_vars.rbegin(); it != m_str_vars.rend(); ++it) {
         auto found = it->find(name);
         if (found != it->end()) return found->second;
      }
      return false;
   }


   // ========================================================================
   // Block / Instruction helpers
   // ========================================================================
   void emit(IRInstruction instruction) { m_block->instructions.push_back(std::move(instruction)); }
   VReg fresh(IRType type) { return m_function->fresh_vreg(type); }

   /**
    * Create a new block, return its id (looked up by id to repoint m_block,
    * as emplace_back into m_function->blocks could reallocate and invalidate
    * any held reference / pointer - so address blocks by id, not by pointer).
    */
   int make_block(const std::string& label) {
      m_function->new_block(label); 
      return m_function->blocks.back().id;
   }


   /**
    * Point m_block at the block with the given id.
    * Must be called after any block creation, since creating a block can 
    * reallocate the vector.
    */
   void switch_to(int block_id) {
      for (auto& block : m_function->blocks)
         if (block.id == block_id) { m_block = &block; return; }
   }


   /**
    * Emit an unconditional branch to 'target' - but only if current block
    * ISN'T already terminated (e.g. the body ended in a return).
    */
   void branch_to(int target) {
      if (m_block->terminated()) return;
      IRInstruction branch(IROp::Br);
      branch.operands.push_back(IROperand::make_block(target));
      emit(branch);
   }

   // ----- top level --------------------------------------------------------
   IRFunction lower_function(const NodeFunction* function);
   void lower_scope(const NodeScopeBlock* block);

   /** TODO: structs go here */

   // ----- statements -------------------------------------------------------
   void lower_stmt(const NodeStmt* stmt);
   void lower_print(const NodeStmtPrint* stmt);
   void lower_exit(const NodeStmtExit* stmt);

   // ----- control flow -----------------------------------------------------
   // if (cond) { then } [else { else }]
   //
   //    <current block>:
   //       ... compute condition ...
   //       condbr condition ? then : (else or end)
   //    then:
   //       ... then body ...
   //       br end (unless already returned)
   //    else:     (only if there's an else)
   //       ... else body ...
   //       br end (unless returned)
   //    end:
   //       ... continues here ... <- m_block repointed here on exit
   void lower_if(const NodeStmtIf* stmt);

   //    while (cond) { body }
   //       <current block>:
   //          br cond_block
   //       cond:
   //          ... compute cond ...
   //          condbr cond ? body : end
   //       body:
   //          ... body ...
   //          br cond (loop back, unless it returned)
   //       end:
   //          ... continues here ...
   void lower_while(const NodeStmtWhile* stmt);

   void lower_for(const NodeStmtFor* stmt);

   // Lower 'cond" so control flows to 'true_id' if true, else 'false_id.
   void lower_condition(const NodeCondition* cond, int true_id, int false_id);


   // ----- lvalue address: the ADDRESS a store targets (ident or indx)
   VReg lower_lvalue_address(const NodeExpr* target);

   // ----- expressions: return the VReg holding the result ------------------
   VReg lower_expr(const NodeExpr* expr);


   // ----- str helper -------------------------------------------------------
   std::pair<std::string, int64_t> intern_string(const std::string& text);
};

#endif // LOWERER_H