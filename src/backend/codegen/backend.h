#ifndef BACKEND_H
#define BACKEND_H

#include <sstream>
#include <string>
#include <unordered_map>

#include "Core/IRDefs.h"

// ===========================================================================
// Backend - turns an IRModule into x86-64 NASM text.
//
// Every VReg gets a fixed 8-byte spill slot on the callee's frame; values are
// always round-tripped through memory (load operands, compute, store dest) -
// no register allocation yet. This keeps codegen for each IROp trivial and
// correct-by-construction, at the cost of speed (fine for a not-yet-optimized
// path). Slots always hold a clean, fully-widened 64-bit value regardless of
// the VReg's IRType; the only place byte-vs-qword width actually matters is
// where we dereference addressable program memory (Load/Store on an Alloca
// buffer, a global, or a struct field) - so that's the only place codegen
// looks at IRType to pick an access width.
// ===========================================================================
class Backend {
public:
   std::string generate(const IRModule& module);

private:
   std::ostringstream m_output;

   // ----- per-function state (reset in gen_function) -----------------------
   const IRFunction*        m_func = nullptr;
   std::unordered_map<int, int> m_slot;      // vreg id -> stack offset (bytes below rbp)
   int m_frame_size  = 0;
   int m_spill_area  = 0; // bytes reserved for vreg slots; alloca buffers start right after
   int m_alloca_used = 0; // running cursor within the alloca-buffer region, reset per function

   int m_rax_holds = -1, m_rbx_holds = -1; // cache that tracks what vreg id is already
                                           // loaded into the scratch registers.

   void plan_frame(const IRFunction& fn);
   void gen_function(const IRFunction& fn);
   void gen_block(const IRBasicBlock& block);
   void gen_instruction(const IRInstruction& instr);
   void gen_binop(const IRInstruction& instr, const std::string& reg1, const std::string& reg2);

   // ----- operand / slot helpers --------------------------------------------
   std::string slot(VReg v) const;                    // "[rbp-N]"
   std::string block_label(const IRBasicBlock& b) const;
   std::string block_label_by_id(int id) const;

   void load_operand(const IROperand& op, const std::string& reg); // reg <- operand (64-bit)
   void load_reg_slot(VReg v, const std::string& reg);             // reg <- [slot(v)]
   void store_reg(VReg dest, const std::string& reg);              // [slot(dest)] <- reg

   // ----- cache helpers -----------------------------------------------------
   void set_holds(const std::string& reg, int vreg_id);
   void clear_cache() { m_rax_holds = m_rbx_holds = -1; }

   void gen_call(const IRInstruction& instr);
   void gen_getelemptr(const IRInstruction& instr);
   void gen_load(const IRInstruction& instr);
   void gen_store(const IRInstruction& instr);
   void gen_cmp(const IRInstruction& instr, const char* setcc);

   // ----- module-level sections ---------------------------------------------
   std::unordered_map<std::string, bool> m_used_routines = {
      { "print_int",  false }, { "print_char", false }, { "print_str", false },
      { "print_nl",   false }, { "sys_exit",   false },
      { "read_int",   false }, { "read_char",  false }, { "read_str",  false },
   };

   void scan_used_routines(const IRModule& module);
   void emit_data_section(const IRModule& module);
   void emit_used_runtime();

   static std::string runtime_print_int();
   static std::string runtime_print_char();
   static std::string runtime_print_str();
   static std::string runtime_print_nl();
   static std::string runtime_sys_exit();
   static std::string runtime_read_int();
   static std::string runtime_read_char();
   static std::string runtime_read_str();
};

#endif // BACKEND_H
