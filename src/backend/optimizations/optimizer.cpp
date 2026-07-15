#include "optimizer.h"

#include <stddef.h>
#include <set>
#include <vector>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "utils/msc.h"

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
      const std::string& current = lines[i];
      const std::string* next = (i + 1 < lines.size() ? &lines[i+1] : nullptr);

      // try to match push/pop pair in adj lines (cur + next)
      std::string current_register, next_register;
      if (next && is_push(current, current_register) && is_pop(*next, next_register)) {
         if (current_register == next_register) { i++; continue; }    // Pattern 1: push X / pop X -> delete both & skip line.
         else {                                  // Pattern 2: push X / pop y -> mov y, X
            output.push_back("   mov " + next_register + ", " + current_register);
            i++; continue;
         }
      }
      else if (is_noop(current) && (!next || !is_cond(*next))) continue;
      else if (try_to_xor(current, current_register) && (!next || !is_cond(*next))) { output.push_back(current_register); continue; }
      else if (same_xor(current, current_register)) { output.push_back(current_register); continue; }
      else if (is_ret(current)) {
         output.push_back(current);
         while (i + 1 < lines.size() && !is_boundary(lines[i + 1])) i++;
         continue;
      }
      output.push_back(current);
   }

   std::string end_res;
   for (auto& line : output) end_res += line + "\n";

   return end_res;
}


bool Optimizer::same_xor(const std::string& line, std::string& out) {
   std::string token = line;
   trim(token);
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }

   if (!token.starts_with("xor ")) return false;
   std::string rest = token.substr(4);
   size_t comma = rest.find(',');
   if (comma == std::string::npos) return false;

   std::string _register = rest.substr(0, comma);
   trim(_register);
   std::string value = rest.substr(comma + 1);
   trim(value);

   if (value != _register) return false;
   if (!is_register(_register) || !is_register(value)) return false;
   auto table = reg_map.find(_register);
   out = "   xor " + (*table).second._32_Bit + ", " + (*table).second._32_Bit;
   return true;
}


bool Optimizer::try_to_xor(const std::string& line, std::string& out) {
   std::string token = line;
   trim(token);
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }

   if (!token.starts_with("mov ")) return false;
   std::string rest = token.substr(4);
   size_t comma = rest.find(',');
   if (comma == std::string::npos) return false;

   std::string _register = rest.substr(0, comma);
   trim(_register);
   std::string value = rest.substr(comma + 1);
   trim(value);
   
   if (value != "0") return false;
   if (!is_register(_register)) return false;
   auto table = reg_map.find(_register);
   out = "   xor " + (*table).second._32_Bit + ", " + (*table).second._32_Bit;
   return true;
}


bool Optimizer::is_ret(const std::string& line) {
   std::string token = line;
   trim(token);
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }

   if (token.empty()) return false;
   return token.starts_with("ret") && token.ends_with("ret");
}


bool Optimizer::is_boundary(const std::string& line) {
   std::string token = line;
   trim(token);
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }
   if (token.empty()) return false;
   
   if (token.back() == ':') return true;
   if (token.starts_with("section ")) return true;
   if (token.front() == '.') return true;
   return false;
}


bool Optimizer::is_push(const std::string& line, std::string& _register) {
   // match "   push <reg>" - trim, check prfx, extract op
   const std::string keyword = "push ";
   auto position = line.find(keyword);
   if (position == std::string::npos) return false;
   // make sure the instruction is not part of something else (comment, etc).
   for (size_t i = 0; i < position; i++)
      if (!std::isspace((unsigned char)line[i])) return false;

   std::string rest = line.substr(position + keyword.length());
   size_t end = rest.find_first_of(" \t;");
   if (end != std::string::npos) rest = rest.substr(0, end);
   trim(rest);
   _register = rest;

   return is_register(_register);
}


bool Optimizer::is_pop(const std::string& line, std::string& _register) {
   const std::string keyword = "pop ";
   auto position = line.find(keyword);
   if (position == std::string::npos) return false;
   // make sure the instruction is not part of something else (comment, etc).
   for (size_t i = 0; i < position; i++)
      if (!std::isspace((unsigned char)line[i])) return false;


   std::string rest = line.substr(position + keyword.length());
   size_t end = rest.find_first_of(" \t;");
   if (end != std::string::npos) rest = rest.substr(0, end);
   trim(rest);
   _register = rest;
   return is_register(_register);
}


bool Optimizer::is_cond(const std::string& string) {
   static const std::set<std::string> conditions {
      "jne", "je", "jge", "jle", "jg", "jl", "cmp"
   };

   std::string token = string;
   trim(token);
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }

   size_t split_point = token.find_first_of(" \t");
   std::string mnemonic = (split_point == std::string::npos) ? token : token.substr(0, split_point);
   return conditions.count(mnemonic) > 0;
}


bool Optimizer::is_register(const std::string& string) {
   static const std::set<std::string> registers = {
      "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
   };
   return registers.count(string) > 0;
}


/** NOTE: relies on identity ops only appearing in address computation, should probably change */
bool Optimizer::is_noop(const std::string& string) {
   std::string token = string;
   trim(token);
   // strip comment
   size_t semicolon = token.find(';');
   if (semicolon != std::string::npos) { token = token.substr(0, semicolon); trim(token); }

   // match basically noop instructions
   if (token.ends_with(", 0") && (token.starts_with("add ") || token.starts_with("sub ")))
      return true;
   if (token.ends_with(", 1") && token.starts_with("imul "))
      return true;
   return false;
}


void Optimizer::trim(std::string& string) {
   size_t start = string.find_first_not_of(" \t\r\n");
   size_t end   = string.find_last_not_of(" \t\r\n");
   if (start == std::string::npos) { string.clear(); return; } // no whitespace
   string = string.substr(start, end - start + 1);
}