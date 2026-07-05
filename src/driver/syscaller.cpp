#include "utils/flags.h"
#include "syscaller.h"
#include <cstdlib>

bool Syscaller::nasm() {
   std::string call = "nasm -felf64 " + m_asm;
   return !system(call.c_str());
}


bool Syscaller::linker() {
   std::string call = "ld " + m_obj + " -o " + m_name;
   return !system(call.c_str());
}


void Syscaller::cleanup() {
   std::string rm = "rm " + m_asm;
   if (!m_opts.keep_asm) system(rm.c_str());
   rm.clear(); rm = "rm " + m_obj;
   if (!m_opts.keep_obj) system(rm.c_str());
   rm.clear(); rm = "rm " + m_name + "_preop.asm";
   if (!m_opts.keep_preop) system(rm.c_str());
}