#ifndef MSC_H
#define MSC_H

template <class> inline constexpr bool always_false = false;

#include <string>
#include <unordered_map>
#include <vector>

struct _8_Bits {
   std::string high;
   std::string low;
};


struct Reg {
   std::string _64_Bit;
   std::string _32_Bit;
   std::string _16_Bit;
   _8_Bits _8_bits;
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

#endif // MSC_H