#include "IRDebug.h"

#include <stddef.h>
#include <vector>

#include "IRDefs.h"


std::string IRPrinter::reg(const VReg& vreg) const {
   if (!vreg.valid()) return "<novalue>";
   return "%" + std::to_string(vreg.id);
}


std::string IRPrinter::operand(const IROperand& operand) const {
   switch (operand.kind) {
      case IROperand::Kind::Reg:      return reg(operand.reg);
      case IROperand::Kind::ConstInt: return std::to_string(operand.const_int);
      case IROperand::Kind::Symbol:   return "@" + operand.symbol;
      case IROperand::Kind::Block:    return "block" + std::to_string(operand.block_id);
      default:                        return "<none>";
   }
}


void IRPrinter::print(const IRModule& module) {
   for (const IRFunction& function : module.functions) {
      print_function(function);
      m_out << "\n";
   }
}


void IRPrinter::print_function(const IRFunction& function) {
   m_out << "func @" << function.name << "(";
   for (size_t i = 0; i < function.params.size(); i++) {
      if (i) m_out << ", ";
      m_out << reg(function.params[i]);
   }
   m_out << ") -> " << ir_type_str(function.ret_type);

   if (function.is_declaration) { m_out << ";   ; extern\n"; return; }

   m_out << " {\n";
   for (const IRBasicBlock& block: function.blocks)
      print_block(block);
   m_out << "}\n";
}


void IRPrinter::print_block(const IRBasicBlock& block) {
   m_out << block.label << ":";
   if (!block.terminated())
      m_out << "   ; WARNING: block not terminated.";
   m_out << "\n";
   for (const IRInstruction& instruction : block.instructions)
      print_instruction(instruction);
}


bool IRPrinter::operatorless(IROp op) {
   return op == IROp::Alloca || op == IROp::CallResult || op == IROp::Exit || op == IROp::Ret; // Ret is optional
}


void IRPrinter::print_instruction(const IRInstruction& instruction) {
   m_out << "   ";

   // dest = ... for value-producing instructions.
   if (instruction.dest.valid())
      m_out << reg(instruction.dest) << " = ";
   
   if (instruction.operands.empty() && !operatorless(instruction.op)) { m_out << "NO OPERANDS IN INSTRUCTION OF TYPE: " << ir_op_str(instruction.op) << std::endl; }
   else
   /** This switch stmt */
   switch (instruction.op) {
      case IROp::Const:
         m_out << "const " << ir_type_str(instruction.dest.type) << " " << instruction.operands[0].const_int;
         break;
      
      case IROp::Copy:
         m_out << "copy " << operand(instruction.operands[0]);
         break;
      
      case IROp::Add: case IROp::Sub: case IROp::Mul: case IROp::Div: case IROp::Mod:
      case IROp::CmpEq: case IROp::CmpNe: case IROp::CmpLt:
      case IROp::CmpLe: case IROp::CmpGe: case IROp::CmpGt:
      case IROp::And: case IROp::Or:
         if (instruction.operands.size() < 2) { m_out << "Missing operand for BinExpr / CmpExpr instruction.\n"; return; }
         m_out << ir_op_str(instruction.op) << " " << ir_type_str(instruction.dest.type) << " "
               << operand(instruction.operands[0]) << ", " << operand(instruction.operands[1]);
         break;

      case IROp::Not:
         m_out << "not " << operand(instruction.operands[0]);
         break;
      
      case IROp::Alloca:
         m_out << "alloca " << ir_type_str(instruction.dest.type) << " (size " << instruction.imm << ")";
         break;
         
      case IROp::Load:
         m_out << "load " << ir_type_str(instruction.dest.type) << " " << operand(instruction.operands[0]);
         break;
      
      case IROp::Store:
         if (instruction.operands.size() < 2) { m_out << "Missing operand for store instruction.\n"; return; }
         m_out << "store " << operand(instruction.operands[1]) << " -> " << operand(instruction.operands[0]);
         break;
      
      case IROp::GetElemPtr:
         if (instruction.operands.size() < 2) { m_out << "Missing operand for getelemptr instruction.\n"; return; }
         m_out << "getelemptr " << operand(instruction.operands[0])
               << "[" << operand(instruction.operands[1]) << " * " << instruction.imm << "]";
            break;
      case IROp::Call:
         m_out << "call " << operand(instruction.operands[0]) << "(";
         for (size_t i = 0; i < instruction.operands.size(); i++) {
            if (i > 1) m_out << ", ";
            m_out << operand(instruction.operands[i]);
         }
         m_out << ")";
         break;

      case IROp::CallResult:
         m_out << "call_result   ; rdx of preceding call";
         break;

      case IROp::CondBr:
         m_out << "condbr " << operand(instruction.operands[0])
               << " ? " << ((instruction.operands.size() > 1) ? operand(instruction.operands[1]) : "NULL OPERAND 1")
               << " : " << (instruction.operands.size() > 2 ? operand(instruction.operands[2]) : "NULL OPERAND 2");
         break;

      case IROp::Ret:
         m_out << "ret";
         if (!instruction.operands.empty()) m_out << " " << operand(instruction.operands[0]);
         break;

      case IROp::Br:
         m_out << "br ";
         if (!instruction.operands.empty()) m_out << operand(instruction.operands[0]);
         else m_out << "<no target!>";
         break;

      default:
         m_out << ir_op_str(instruction.op);
         break;
   }
   m_out << "\n";
}