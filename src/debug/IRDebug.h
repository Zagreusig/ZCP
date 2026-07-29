#ifndef IRDEBUG_H
#define IRDEBUG_H

#include "Core/IRDefs.h"
#include <ostream>
#include <sstream>
#include <string>

// ===========================================================================
// IRPrinter - renders IR as text. For debugging sob
//
// func @main() -> i64 {
// entry:
//    %0 = const i64 3
//    %1 = const i64 4
//    %2 = mul i64 %0, %1
//    %3 = alloca i64
//    store %2 -> %3
//    %4 = load i64 %3
//    ret %4
// }
//
// VRegs print as %id, blocks as their label, constants inline.
// ===========================================================================

class IRPrinter {
public:
   explicit IRPrinter(std::ostream& out) : m_out(out) {}

   void print(const IRModule& module);
   void print_function(const IRFunction& function);
private:
   std::ostream& m_out;

   std::string reg(const VReg& vreg) const;
   std::string operand(const IROperand& operand) const;

   void print_block(const IRBasicBlock& block);
   void print_instruction(const IRInstruction& instruction);
};

#endif // IRDEBUG_H