#include "backend.h"

#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>
#include <stdexcept>

#include "IRDefs.h"
#include "utils/msc.h"

static const char* arg_regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };


std::string Backend::generate(const IRModule& module) {
   m_output.str("");

   scan_used_routines(module);


   /** TODO: simply don't emit these if their corresponding print/read routines aren't emitted. */
   m_output << "section .bss\n"
            << "   IO_buf resb 256\n\n"; // I/O routine scratch buffer

   m_output << "section .text\n";
   emit_used_runtime();

   m_output << "global _start\n"
            << "_start:\n"
            << (module.functions.size() > 0 ? "   call main\n" : "") /** TODO: this needs to change */
            << "   mov rdi, rax\n"
            << "   mov rax, 60\n"
            << "   syscall\n\n";

 
   try {
      for (const IRFunction& fn : module.functions) {
         if (fn.is_declaration) continue;
         gen_function(fn);
      }
   }
   catch (const std::runtime_error& e) {
      std::cerr << "Backend RTE: " << e.what() << std::endl;
      return m_output.str();
   }
   catch (const std::logic_error& e) {
      std::cerr << "Backend LogE: " << e.what() << std::endl;
      return m_output.str();
   }
   emit_data_section(module);

   return m_output.str();
}


// ===========================================================================
// Frame planning: every VReg that appears as a `dest` anywhere in the
// function gets an 8-byte spill slot. Every Alloca additionally reserves
// `imm` bytes of raw buffer space (the buffer's address is what gets written
// into that Alloca's dest slot). Slots and buffers coexist below rbp; the
// arrangement (spill slots first, buffers after) is arbitrary but must be
// computed the same way here and consumed the same way in gen_instruction's
// Alloca case, in the same block/instruction order.
// ===========================================================================
void Backend::plan_frame(const IRFunction& fn) {
   m_slot.clear();
   int next_slot_id = 0;
   auto assign_slot = [&](VReg v) {
      if (!v.valid()) return;
      if (m_slot.count(v.id)) return;
      m_slot[v.id] = (next_slot_id + 1) * 8;
      next_slot_id++;
   };

   for (VReg p : fn.params) assign_slot(p);
   for (const IRBasicBlock& block : fn.blocks)
      for (const IRInstruction& instr : block.instructions)
         if (instr.dest.valid()) assign_slot(instr.dest);

   int spill_area = next_slot_id * 8;

   int buffer_area = 0;
   for (const IRBasicBlock& block : fn.blocks)
      for (const IRInstruction& instr : block.instructions)
         if (instr.op == IROp::Alloca) buffer_area += INT(instr.imm);

   m_spill_area = spill_area;
   m_frame_size = (spill_area + buffer_area + 15) & ~15;
}


std::string Backend::slot(VReg v) const {
   auto it = m_slot.find(v.id);
   return "[rbp-" + std::to_string(it->second) + "]";
}


std::string Backend::block_label_by_id(int id) const {
   return "." + m_func->name + "_L" + std::to_string(id);
}


std::string Backend::block_label(const IRBasicBlock& b) const {
   return block_label_by_id(b.id);
}


void Backend::set_holds(const std::string& reg, int vreg_id) {
   if      (reg == "rax") m_rax_holds = vreg_id;
   else if (reg == "rbx") m_rbx_holds = vreg_id;
}


void Backend::load_operand(const IROperand& op, const std::string& reg) {
   if (op.kind == IROperand::Kind::Reg) {
      int want = op.reg.id;
      // already in trgt register 
      if (reg == "rax" && m_rax_holds == want) return;
      if (reg == "rbx" && m_rbx_holds == want) return;

      // in OTHER register?
      if (reg == "rax" && m_rbx_holds == want)
         { m_output << "   mov rax, rbx\n"; m_rax_holds = want; return; }
      if (reg == "rbx" && m_rax_holds == want)
         { m_output << "   mov rbx, rax\n"; m_rbx_holds = want; return; }

      // not cached: real load and bookkeeping
      m_output << "   mov " << reg << ", " << slot(op.reg) << "\n";
      set_holds(reg, want);
   }
   else if (op.kind == IROperand::Kind::ConstInt) {
      m_output << "   mov " << reg << ", " << op.const_int << "\n";
      set_holds(reg, -1); // const isn't a VReg, clear cache
   }
   else if (op.kind == IROperand::Kind::Symbol)
      m_output << "   lea " << reg << ", [" << op.symbol << "]\n";
}


void Backend::load_reg_slot(VReg v, const std::string& reg) {
   m_output << "   mov " << reg << ", " << slot(v) << "\n";
}


void Backend::store_reg(VReg dest, const std::string& reg) {
   m_output << "   mov " << slot(dest) << ", " << reg << "\n";
   set_holds(reg, dest.id); // cached result
}


void Backend::gen_function(const IRFunction& fn) {
   m_func = &fn;
   plan_frame(fn);
   m_alloca_used = 0;

   m_output << "\n" << fn.name << ":\n"
            << "   push rbp\n"
            << "   mov rbp, rsp\n";
   if (m_frame_size > 0) m_output << "   sub rsp, " << m_frame_size << "\n";

   for (size_t i = 0; i < fn.params.size(); i++) {
      if (i >= 6) break; // stack-passed params: not supported yet
      store_reg(fn.params[i], arg_regs[i]);
   }

   for (const IRBasicBlock& block : fn.blocks)
      gen_block(block);

   m_func = nullptr;
}


void Backend::gen_block(const IRBasicBlock& block) {
   clear_cache(); // m_rax_holds & m_rbx_holds = -1;
   m_output << block_label(block) << ":\n";
   for (const IRInstruction& instr : block.instructions)
      gen_instruction(instr);
}


void Backend::gen_cmp(const IRInstruction& instr, const char* setcc) {
   load_operand(instr.operands[0], "rax");
   if (instr.operands[1].kind != IROperand::Kind::ConstInt) {
      load_operand(instr.operands[1], "rbx");
      m_output << "   cmp rax, rbx\n";
   }
   else 
      m_output << "   cmp rax, " << instr.operands[1].const_int << "\n"; // Emit instr directly, rather than load into rbx
   
   m_output << "   " << setcc << " al\n"
            << "   movzx rax, al\n";
   store_reg(instr.dest, "rax");
}


void Backend::gen_getelemptr(const IRInstruction& instr) {
   // dest = operands[0](base ptr value) + operands[1](index, reg or const) * imm
   load_operand(instr.operands[0], "rax");
   load_operand(instr.operands[1], "rbx");
   m_output << "   imul rbx, " << instr.imm << "\n"
            << "   add rax, rbx\n";
   store_reg(instr.dest, "rax");
}


void Backend::gen_load(const IRInstruction& instr) {
   load_operand(instr.operands[0], "rax"); // rax = address
   if (instr.dest.type == IRType::I8)
      m_output << "   movzx rbx, byte [rax]\n";
   else
      m_output << "   mov rbx, [rax]\n";
   store_reg(instr.dest, "rbx");
}


void Backend::gen_store(const IRInstruction& instr) {
   load_operand(instr.operands[0], "rax"); // rax = address
   load_operand(instr.operands[1], "rbx"); // rbx = value
   if (instr.operands[1].kind == IROperand::Kind::Reg && instr.operands[1].reg.type == IRType::I8)
      m_output << "   mov byte [rax], bl\n";
   else
      m_output << "   mov qword [rax], rbx\n";
}


void Backend::gen_call(const IRInstruction& instr) {
   size_t arg_start = 1; // operands[0] is the callee symbol
   size_t nargs = instr.operands.size() - arg_start;
   if (nargs > 6) { std::cerr << "backend: too many call args (>6 unsupported)\n"; std::exit(1); }

   for (size_t i = 0; i < nargs; i++)
      load_operand(instr.operands[arg_start + i], arg_regs[i]);

   m_output << "   call " << instr.operands[0].symbol << "\n";

   if (instr.dest.valid()) {
      if (instr.dest.type == IRType::I8) m_output << "   movzx rax, al\n";
      store_reg(instr.dest, "rax");
   }
}


void Backend::gen_binop(const IRInstruction& instr, const std::string& reg1, const std::string& reg2) {
   load_operand(instr.operands[0], "rax");
   bool const_int = instr.operands[1].kind == IROperand::Kind::ConstInt;
   if (!const_int) load_operand(instr.operands[1], "rbx");
   
   switch (instr.op) {
      case IROp::Add:
         m_output << "   add " << reg1 << ", "; break;
      case IROp::Sub:
         m_output << "   sub " << reg1 << ", "; break;
      case IROp::Mul:
         m_output << "   imul " << reg1 << ", "; break;
      case IROp::Div:
      case IROp::Mod:
         m_output << "   cqo\n   idiv ";
         m_output << (const_int ? std::to_string(instr.operands[1].const_int) : reg2);
         break;
      default: return;
   }

   if (instr.op == IROp::Div || instr.op == IROp::Mod)
      m_output << reg1 << "\n";
   else
      m_output << (const_int ? std::to_string(instr.operands[1].const_int) : reg2) << "\n"; 

   store_reg(instr.dest, (instr.op == IROp::Mod ? "rdx" : "rax"));
}


void Backend::gen_instruction(const IRInstruction& instr) {
   switch (instr.op) {
      case IROp::Const:
         m_output << "   mov rax, " << instr.operands[0].const_int << "\n";
         store_reg(instr.dest, "rax");
         break;

      case IROp::Copy:
         load_operand(instr.operands[0], "rax");
         store_reg(instr.dest, "rax");
         break;

      case IROp::Add: case IROp::Sub:
      case IROp::Mul: case IROp::Div: case IROp::Mod:
         gen_binop(instr, "rax", "rbx"); break;

      case IROp::CmpEq: gen_cmp(instr, "sete");  break;
      case IROp::CmpNe: gen_cmp(instr, "setne"); break;
      case IROp::CmpLt: gen_cmp(instr, "setl");  break;
      case IROp::CmpLe: gen_cmp(instr, "setle"); break;
      case IROp::CmpGt: gen_cmp(instr, "setg");  break;
      case IROp::CmpGe: gen_cmp(instr, "setge"); break;

      case IROp::And:
         load_operand(instr.operands[0], "rax"); load_operand(instr.operands[1], "rbx");
         m_output << "   and rax, rbx\n"; store_reg(instr.dest, "rax");
         break;
      case IROp::Or:
         load_operand(instr.operands[0], "rax"); load_operand(instr.operands[1], "rbx");
         m_output << "   or rax, rbx\n"; store_reg(instr.dest, "rax");
         break;
      case IROp::Not:
         load_operand(instr.operands[0], "rax");
         m_output << "   cmp rax, 0\n   sete al\n   movzx rax, al\n";
         store_reg(instr.dest, "rax");
         break;

      case IROp::Alloca: {
         m_alloca_used += INT(instr.imm);
         int offset = m_spill_area + m_alloca_used; // low address of this buffer
         m_output << "   lea rax, [rbp-" << offset << "]\n";
         store_reg(instr.dest, "rax");
         break;
      }

      case IROp::Load:  gen_load(instr);  break;
      case IROp::Store: gen_store(instr); break;

      case IROp::GlobalAddr:
         m_output << "   lea rax, [" << instr.operands[0].symbol << "]\n";
         store_reg(instr.dest, "rax");
         break;

      case IROp::GetElemPtr: gen_getelemptr(instr); break;

      case IROp::Call: gen_call(instr); break;

      case IROp::CallResult:
         store_reg(instr.dest, "rdx");
         break;

      case IROp::Br:
         m_output << "   jmp " << block_label_by_id(instr.operands[0].block_id) << "\n";
         break;

      case IROp::CondBr:
         load_operand(instr.operands[0], "rax");
         m_output << "   cmp rax, 0\n"
                  << "   jne " << block_label_by_id(instr.operands[1].block_id) << "\n"
                  << "   jmp " << block_label_by_id(instr.operands[2].block_id) << "\n";
         break;

      case IROp::Ret:
         if (!instr.operands.empty()) load_operand(instr.operands[0], "rax");
         if (instr.operands.size() > 1) load_operand(instr.operands[1], "rdx");
         m_output << "   mov rsp, rbp\n   pop rbp\n   ret\n";
         break;

      case IROp::Exit:
         // preceded by a diverging sys_exit Call; nothing to emit.
         break;
   }
}


// ===========================================================================
// Module-level sections
// ===========================================================================
void Backend::emit_data_section(const IRModule& module) {
   m_output << "\nsection .data\n";
            // << "   nl_char db 10\n"; unnecessary

   for (const IRGlobal& g : module.globals) {
      m_output << "   " << g.label << ": db ";
      if (g.bytes.empty()) {
         m_output << "0\n";
         continue;
      }
      for (size_t i = 0; i < g.bytes.size(); i++) {
         if (i) m_output << ", ";
         m_output << INT(g.bytes[i]);
      }
      m_output << "\n";
   }
}


void Backend::emit_used_runtime() {
   for (auto& routine : m_used_routines) {
      if (!routine.second) continue;

      if      (routine.first == "print_int")
         m_output << runtime_print_int();
      else if (routine.first == "print_char")
         m_output << runtime_print_char();
      else if (routine.first == "print_str")
         m_output << runtime_print_str();
      else if (routine.first == "print_nl")
         m_output << runtime_print_nl();
      else if (routine.first == "read_char")
         m_output << runtime_read_char();
      else if (routine.first == "read_int")
         m_output << runtime_read_int();
      else if (routine.first == "read_str")
         m_output << runtime_read_str();
      else if (routine.first == "sys_exit")
         m_output << runtime_sys_exit();
   }
}


/** TODO: Figure out a better way to do this, could REALLY eat performance. 
 *  condition in a loop in a loop in a loop */
void Backend::scan_used_routines(const IRModule& module) {
   for (auto& function : module.functions)
      for (auto& block : function.blocks)
         for (auto& step : block.instructions) 
            if (step.op == IROp::Call && m_used_routines.contains(step.operands[0].symbol))
               m_used_routines[step.operands[0].symbol] = true;
}


std::string Backend::runtime_print_int() {
   return
"print_int:                     ; rdi = num, rsi = newline flag (0/1)\n"
"   mov r11, rsi                ; save flag, rsi gets clobbered below\n"
"   mov rax, rdi\n"
"   mov byte [IO_buf+31], 10\n"
"   lea r10, [IO_buf+30]\n"
"   mov rcx, 10\n"
"   mov r8, 0\n"
"   test rax, rax\n"
"   jnz .pi_sign\n"
"   mov byte [r10], '0'\n"
"   dec r10\n"
"   jmp .pi_write\n"
".pi_sign:\n"
"   jns .pi_convert\n"
"   mov r8, 1\n"
"   neg rax\n"
".pi_convert:\n"
"   xor rdx, rdx\n"
"   div rcx\n"
"   add dl, '0'\n"
"   mov [r10], dl\n"
"   dec r10\n"
"   test rax, rax\n"
"   jnz .pi_convert\n"
"   cmp r8, 1\n"
"   jne .pi_write\n"
"   mov byte [r10], '-'\n"
"   dec r10\n"
".pi_write:\n"
"   inc r10\n"
"   lea rdx, [IO_buf+31]\n"
"   sub rdx, r10\n"
"   add rdx, r11\n"
"   mov rax, 1\n"
"   mov rdi, 1\n"
"   mov rsi, r10\n"
"   syscall\n"
"   ret\n\n";
}


std::string Backend::runtime_print_char() {
   return
"print_char:                   ; rdi = char, rsi = newline flag (0/1)\n"
"   push rbp\n"
"   mov rbp, rsp\n"
"   sub rsp, 16\n"                // stack-local 2-byte buffer (16 for alignment)
"   mov byte [rbp - 2], dil\n"    // char into the buffer
"   test sil, sil\n"
"   jz .print_char_one\n"
"   mov byte [rbp - 1], 10\n"     // '\n' right after it
"   mov rdx, 2\n"                 // length 2
"   jmp .print_char_write\n"
".print_char_one:\n"
"   mov rdx, 1\n"                 // length 1
".print_char_write:\n"
"   lea rsi, [rbp - 2]\n"         // buffer address
"   mov rax, 1\n"
"   mov rdi, 1\n"
"   syscall\n"
"   mov rsp, rbp\n"
"   pop rbp\n"
"   ret\n\n";
}


std::string Backend::runtime_print_str() {
   return
"print_str:                    ; rdi = ptr, rsi = len, rdx = newline flag\n"
"   push rbp\n"
"   mov rbp, rsp\n"
"   push rdx\n"                    // save newline flag across the syscall
"   mov rdx, rsi\n"                // rdx = len (write's 3rd arg)
"   mov rsi, rdi\n"                // rsi = ptr (write's 2nd arg)
"   mov rax, 1\n"
"   mov rdi, 1\n"
"   syscall\n"
"   pop rdx\n"                    // restore newline flag
"   test dl, dl\n"
"   jz .print_str_done\n"
"   sub rsp, 16\n"                // stack-local 1-byte buffer (16 for alignment)
"   mov byte [rbp - 8], 10\n"
"   lea rsi, [rbp - 8]\n"
"   mov rdx, 1\n"
"   mov rdi, 1\n"
"   mov rax, 1\n"                 // rax clobbered by the syscall above - SYS_write again
"   syscall\n"
".print_str_done:\n"
"   mov rsp, rbp\n"
"   pop rbp\n"
"   ret\n\n";
}


std::string Backend::runtime_print_nl() {
   return
"print_nl:\n"
"   push rbp\n"
"   mov rbp, rsp\n"
"   sub rsp, 16\n"                // stack-local 1-byte buffer (16 for alignment)
"   mov byte [rbp - 8], 10\n"
"   lea rsi, [rbp - 8]\n"
"   mov rdx, 1\n"
"   mov rax, 1\n"
"   mov rdi, 1\n"
"   syscall\n"
"   mov rsp, rbp\n"
"   pop rbp\n"
"   ret\n\n";
}


std::string Backend::runtime_sys_exit() {
   return
"sys_exit:                     ; rdi = exit code (already correct per the exit syscall's ABI)\n"
"   mov rax, 60\n"
"   syscall\n\n";
}


std::string Backend::runtime_read_char() {
   return
"read_char:\n"
"   mov rax, 0\n"
"   mov rdi, 0\n"
"   lea rsi, [IO_buf]\n"
"   mov rdx, 1\n"
"   syscall\n"
"   movzx rax, byte [IO_buf]\n"
"   ret\n\n";
}


std::string Backend::runtime_read_int() {
   return
"read_int:                     ; positive ints only, for now\n"
"   xor rax, rax\n"
".ri_loop:\n"
"   push rax\n"
"   mov rax, 0\n"
"   mov rdi, 0\n"
"   lea rsi, [IO_buf]\n"
"   mov rdx, 1\n"
"   syscall\n"
"   cmp rax, 0\n"
"   pop rax\n"
"   je .ri_done\n"
"   movzx rcx, byte [IO_buf]\n"
"   cmp rcx, 10\n"
"   je .ri_done\n"
"   cmp rcx, '0'\n"
"   jb .ri_done\n"
"   cmp rcx, '9'\n"
"   ja .ri_done\n"
"   sub rcx, '0'\n"
"   mov rdx, rax\n"
"   shl rax, 3\n"
"   shl rdx, 1\n"
"   add rax, rdx\n"
"   add rax, rcx\n"
"   jmp .ri_loop\n"
".ri_done:\n"
"   ret\n\n";
}


std::string Backend::runtime_read_str() {
   return
"read_str:                     ; no args; returns rax = ptr, rdx = len\n"
"   xor rcx, rcx\n"
".rs_loop:\n"
"   mov rax, 0\n"
"   mov rdi, 0\n"
"   lea rsi, [IO_buf]\n"
"   add rsi, rcx\n"
"   mov rdx, 1\n"
"   syscall\n"
"   cmp rax, 0\n"
"   je .rs_done\n"
"   movzx rax, byte [IO_buf + rcx]\n"
"   cmp rax, 10\n"
"   je .rs_done\n"
"   inc rcx\n"
"   cmp rcx, 255\n"
"   jb .rs_loop\n"
".rs_done:\n"
"   lea rax, [IO_buf]\n"
"   mov rdx, rcx\n"
"   ret\n\n";
}
