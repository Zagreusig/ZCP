#include "optimizer.h"
#include "utils/msc.h"
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

void Optimizer::optimize() {
   m_optimized = m_orig;
   std::string prev;

   do {
      prev = m_optimized;
      m_optimized = peephole();
   } while (m_optimized != prev);
}


std::string Optimizer::peephole() {
   // Everything gets split into lines
   std::vector<std::string> lines;
   std::stringstream ss(m_optimized);
   std::string line;
   while (std::getline(ss, line)) lines.push_back(line);

   std::vector<std::string> output;
   for (size_t i = 0; i < lines.size(); i++) {
      const std::string& cur = lines[i];
      const std::string* next = (i + 1 < lines.size() ? &lines[i+1] : nullptr);

      // try to match push/pop pair in adj lines (cur + next)
      std::string creg, nreg;
      if (next && is_push(cur, creg) && is_pop(*next, nreg)) {
         if (creg == nreg) { i++; continue; }    // Pattern 1: push X / pop X -> delete both & skip line.
         else {                                  // Pattern 2: push X / pop y -> mov y, X
            output.push_back("   mov " + nreg + ", " + creg);
            i++; continue;
         }
      }
      else if (is_noop(cur) && (!next || !is_cond(*next))) continue;
      else if (try_to_xor(cur, creg) && (!next || !is_cond(*next))) { output.push_back(creg); continue; }
      else if (is_ret(cur)) {
         output.push_back(cur);
         while (i + 1 < lines.size() && !is_boundary(lines[i + 1])) i++;
         continue;
      }
      output.push_back(cur);
   }

   std::string end_res;
   for (auto& line : output) end_res += line + "\n";

   return end_res;
}


bool Optimizer::try_to_xor(const std::string& line, std::string& out) {
   std::string t = line;
   trim(t);
   size_t c = t.find(';');
   if (c != std::string::npos) { t = t.substr(0, c); trim(t); }

   if (!t.starts_with("mov ")) return false;
   std::string rest = t.substr(4);
   size_t comma = rest.find(',');
   if (comma == std::string::npos) return false;

   std::string reg = rest.substr(0, comma);
   trim(reg);
   std::string val = rest.substr(comma + 1);
   trim(val);
   
   if (val != "0") return false;
   if (!is_register(reg)) return false;
   auto tbl = reg_map.find(reg);
   out = "   xor " + (*tbl).second._32_Bit + ", " + (*tbl).second._32_Bit;
   return true;
}


bool Optimizer::is_ret(const std::string& line) {
   std::string t = line;
   trim(t);
   size_t c = t.find(';');
   if (c != std::string::npos) { t = t.substr(0, c); trim(t); }

   if (t.empty()) return false;
   return t.starts_with("ret") && t.ends_with("ret");
}


bool Optimizer::is_boundary(const std::string& line) {
   std::string t = line;
   trim(t);
   size_t c = t.find(';');
   if (c != std::string::npos) { t = t.substr(0, c); trim(t); }
   if (t.empty()) return false;
   
   if (t.back() == ':') return true;
   if (t.starts_with("section ")) return true;
   if (t.front() == '.') return true;
   return false;
}


bool Optimizer::is_push(const std::string& line, std::string& reg) {
   // match "   push <reg>" - trim, check prfx, extract op
   const std::string kw = "push ";
   auto p = line.find(kw);
   if (p == std::string::npos) return false;
   // make sure the instruction is not part of something else (comment, etc).
   for (size_t i = 0; i < p; i++)
      if (!std::isspace((unsigned char)line[i])) return false;

   std::string rest = line.substr(p + kw.length());
   size_t e = rest.find_first_of(" \t;");
   if (e != std::string::npos) rest = rest.substr(0, e);
   trim(rest);
   reg = rest;

   return is_register(reg);
}


bool Optimizer::is_pop(const std::string& line, std::string& reg) {
   const std::string kw = "pop ";
   auto p = line.find(kw);
   if (p == std::string::npos) return false;
   // make sure the instruction is not part of something else (comment, etc).
   for (size_t i = 0; i < p; i++)
      if (!std::isspace((unsigned char)line[i])) return false;


   std::string rest = line.substr(p + kw.length());
   size_t e = rest.find_first_of(" \t;");
   if (e != std::string::npos) rest = rest.substr(0, e);
   trim(rest);
   reg = rest;
   return is_register(reg);
}


bool Optimizer::is_cond(const std::string& s) {
   static const std::set<std::string> conds {
      "jne", "je", "jge", "jle", "jg", "jl", "cmp"
   };

   std::string t = s;
   trim(t);
   size_t c = t.find(';');
   if (c != std::string::npos) { t = t.substr(0, c); trim(t); }

   size_t sp = t.find_first_of(" \t");
   std::string mnemonic = (sp == std::string::npos) ? t : t.substr(0, sp);
   return conds.count(mnemonic) > 0;
}


bool Optimizer::is_register(const std::string& s) {
   static const std::set<std::string> regs = {
      "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
   };
   return regs.count(s) > 0;
}


/** NOTE: relies on identity ops only appearing in address computation, should probably change */
bool Optimizer::is_noop(const std::string& s) {
   std::string t = s;
   trim(t);
   // strip comment
   size_t c = t.find(';');
   if (c != std::string::npos) { t = t.substr(0, c); trim(t); }

   // match basically noop instructions
   if (t.ends_with(", 0") && (t.starts_with("add ") || t.starts_with("sub ")))
      return true;
   if (t.ends_with(", 1") && t.starts_with("imul "))
      return true;
   return false;
}


void Optimizer::trim(std::string& s) {
   size_t start = s.find_first_not_of(" \t\r\n");
   size_t end   = s.find_last_not_of(" \t\r\n");
   if (start == std::string::npos) { s.clear(); return; } // no whitespace
   s = s.substr(start, end - start + 1);
}