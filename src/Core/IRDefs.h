#ifndef IRDEFS_H
#define IRDEFS_H

#include <cstdint>
#include <string>
#include <vector>

// ===========================================================================
// Types carried by IR values. Meant to be minimal and separate from AST's
// TypeInfo stuff so that the backend doesn't depend on frontend types.
// ===========================================================================

enum class IRType {
   Void,
   I8,   // Char / bool (byte width things)
   I64,  // int
   Ptr   // addresses (string data, array bases, other stuff)
};


inline const char* ir_type_str(IRType t) {
   switch (t) {
      case IRType::Void: return "void";
      case IRType::I8:   return "i8";
      case IRType::I64:  return "i64";
      case IRType::Ptr:  return "ptr";
      default:           return "???";
   }
}


// ===========================================================================
// A virtual register (helpful for eventual SSA). Simply id + type.
// In plain three-address, VReg id can be assigned multiple times; when I
// eventually move to SSA that won't happen. Representing as a small struct as
// opposed to a bare int lets me attach defining instructions later on.
// ===========================================================================
struct VReg {
   int    id = -1;             // -1 = "no reg" (void result)
   IRType type = IRType::Void;

   bool valid() const { return id >= 0; }
};


// ===========================================================================
// An operand to an instruction: either virtual register, integer constant, or
// a symbolic reference (a global/function/label name, or a block trgt). Kept
// as a tagged union-esque struct.
// ===========================================================================
struct IROperand {
   enum class Kind { None, Reg, ConstInt, Symbol, Block } kind = Kind::None;

   VReg        reg;            // Kind::Reg
   int64_t     const_int = 0;  // Kind::ConstInt
   std::string symbol;         // Kind::Symbol (func name, var name, str label)
   int         block_id  = -1; // Kind::Block (branch trgt)

   static IROperand make_reg(VReg r)           { IROperand o; o.kind = Kind::Reg;      o.reg = r;               return o; }
   static IROperand make_const(int64_t v)      { IROperand o; o.kind = Kind::ConstInt; o.const_int = v;         return o; }
   static IROperand make_symbol(std::string s) { IROperand o; o.kind = Kind::Symbol;   o.symbol = std::move(s); return o; }
   static IROperand make_block(int id)         { IROperand o; o.kind = Kind::Block;    o.block_id = id;         return o; }
};


// ===========================================================================
// Op codes. Small on purpose. Each maps to a small number of machine 
// instructions in the backend. Grouped by category.
// ===========================================================================
enum class IROp {
   // consts / moves
   Const,    // dest = const_int   (materialize a constant)
   Copy,     // dest = operand     (register a move)

   // integer arithmetic (dest = a OP b)
   Add, Sub, Mul, Div, Mod,

   // comparisons (dest = a CMP b -> 0/1)
   CmpEq, CmpNe, CmpLt, CmpLe, CmpGt, CmpGe,

   // logical (short-circuit lowered to branches -> 0/1)
   And, Or, Not,

   // Memory: variables in memory, accessed through these.
   //   Alloca : reserve a stack slot for a names local, dest = its addr
   //   Load   : dest = *addr
   //   Store  : *addr = value
   Alloca,  // dest (ptr) = stack slot for local of some IRType / size
   Load,    // dest = load [operand0 (addr)]
   Store,   // store operand1 (value) -> [operand0 (addr)]

   GlobalAddr, // dest(ptr) = address of a named global (operand0 = symbol)

   // indexing / addressing (compute an element address)
   GetElemPtr,  // dest(ptr) = base(operand0) + index(operand1) * elem_size(imm)

   // calls
   Call,       //dest = call symbol(args...) (dest invalid if void)
   CallResult, // dest = rdx from the immediately-preceding Call (STR len half)

   // terminators (each block ends in ONE of these)
   Br,     // unconditional branch to block operand0
   CondBr, // if operand0 != 0 -> block operand1 else block operand2
   Ret,    // return operand0 (or void if none)
   Exit   // exit program
};


inline const char* ir_op_str(IROp op) {
   switch (op) {
      case IROp::Const: return "const"; case IROp::Copy: return "copy";
      case IROp::Add: return "add"; case IROp::Sub: return "sub";
      case IROp::Mul: return "mul"; case IROp::Div: return "div"; case IROp::Mod: return "mod";
      case IROp::CmpEq: return "cmp.eq"; case IROp::CmpNe: return "cmp.ne";
      case IROp::CmpLt: return "cmp.lt"; case IROp::CmpGt: return "cmp.gt";
      case IROp::CmpLe: return "cmp.le"; case IROp::CmpGe: return "cmp.ge";
      case IROp::And: return "and"; case IROp::Or: return "or"; case IROp::Not: return "not";
      case IROp::Alloca: return "alloca"; case IROp::Load: return "load"; case IROp::Store: return "store";
      case IROp::GlobalAddr: return "global_addr";
      case IROp::GetElemPtr: return "getelemptr";
      case IROp::Call: return "call";
      case IROp::Br: return "br"; case IROp::CondBr: return "condbr"; case IROp::Ret: return "ret"; case IROp::Exit: return "exit";
      default: return "????";
   }
}


// Is this opcode a block term?
inline bool ir_IsTerminator(IROp op) {
   return op == IROp::Br || op == IROp::CondBr || op == IROp::Ret || op == IROp::Exit;
}


// ===========================================================================
// One instruction: an opcode, an optional destination VReg, operands, and a
// small immediate (used by Alloca for size, GetElemPtr for element size)
// ===========================================================================
struct IRInstruction {
   IROp op;
   VReg dest;
   std::vector<IROperand> operands;
   int64_t imm = 0;

   IRInstruction(IROp o) : op(o) {}
   IRInstruction(IROp o, VReg d) : op(o), dest(d) {}
};


// ===========================================================================
// A basic block: an id, a label (for readability / emission), and a list of
// instructiond ending in a terminator.
// ===========================================================================
struct IRBasicBlock {
   int id;
   std::string label;
   std::vector<IRInstruction> instructions;

   IRBasicBlock(int i, std::string l) : id(i), label(std::move(l)) {}

   bool terminated() const {
      return !instructions.empty() && ir_IsTerminator(instructions.back().op);
   }
};


// ===========================================================================
// A functionL: name, parameter VRegs, return type, and its basic blocks.
// Owns the fresh-VReg and fresh-block counters used during the lowering.
// ===========================================================================
struct IRFunction {
   std::string       name;
   std::vector<VReg> params;
   IRType            ret_type = IRType::Void;
   bool              is_declaration = false;   // stub (no body) -> extern, for Model B

   std::vector<IRBasicBlock> blocks;

   // fresh-id allocation during lowering.
   int next_vreg  = 0;
   int next_block = 0;

   VReg fresh_vreg(IRType t) {
      VReg v; v.id = next_vreg++; v.type = t; return v;
   }

   IRBasicBlock& new_block(const std::string& label) {
      blocks.emplace_back(next_block++, label);
      return blocks.back();
   }
};


// ===========================================================================
// A module: the whole compilation's functions (later globals, str dat, etc).
// ===========================================================================
struct IRGlobal {
   std::string label;
   std::vector<uint8_t> bytes;
};

struct IRModule {
   std::vector<IRFunction> functions;
   std::vector<IRGlobal> globals; // str lits, etc.
};
#endif // IRDEFS_H