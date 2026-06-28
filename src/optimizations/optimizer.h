#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


struct Reg {
   std::string _64_Bit;
   std::string _32_Bit;
   std::string _16_Bit;
   std::pair<std::string, std::string> _8_Bits;
};

/**
 * rax > rdx - data registers (d)
 * rsi & rdi - index registers
 * rbp & rsp = pointer registers
 */

static const std::unordered_map<std::string, Reg> reg_map = {
   { "rax", { "rax", "eax", "ax",    { "ah", "al" } } },
   { "rbx", { "rbx", "ebx", "bx",    { "bh", "bl" } } },
   { "rcx", { "rcx", "ecx", "cx",    { "ch", "cl" } } },
   { "rdx", { "rdx", "edx", "dx",    { "dh", "dl" } } },
   { "rsi", { "rsi", "esi", "si",    { "",  "sil" } } },
   { "rdi", { "rdi", "edi", "di",    { "",  "dil" } } },
   { "rbp", { "rbp", "ebp", "bp",    { "",  "bpl" } } },
   { "rsp", { "rsp", "esp", "sp",    { "",  "spl" } } },
   { "r8",  { "r8", "r8d", "r8w",    { "",  "r8b" } } },
   { "r9",  { "r9", "r9d", "r9w",    { "",  "r9b" } } },
   { "r10", { "r10", "r10d", "r10w", { "", "r10b" } } },
   { "r11", { "r11", "r11d", "r11w", { "", "r11b" } } },
   { "r12", { "r12", "r12d", "r12w", { "", "r12b" } } },
   { "r13", { "r13", "r13d", "r13w", { "", "r13b" } } },
   { "r14", { "r14", "r14d", "r14w", { "", "r14b" } } },
   { "r15", { "r15", "r15d", "r15w", { "", "r15b" } } }
};

class Optimizer {
public:
   Optimizer(std::string generated) 
      : m_orig(generated) {}

   void optimize();
   std::string peephole();

   // DEBUG
   std::string orig() { return m_orig; }
   std::string finish() { return m_optimized; }
private:
   bool is_push(const std::string&, std::string&);
   bool is_pop(const std::string&, std::string&);
   bool is_register(const std::string&);
   bool is_noop(const std::string&);
   bool is_cond(const std::string&);
   bool is_ret(const std::string&);
   bool is_boundary(const std::string&);
   bool try_to_xor(const std::string&, std::string&);

   void trim(std::string&);

   std::string m_orig;
   std::string m_optimized;
};

#endif // OPTIMIZER_H