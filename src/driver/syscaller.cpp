#include <cstdlib>

#include "syscaller.h"

bool Syscaller::nasm() {
   std::string call = "nasm -felf64 " + m_asm;
   return system(call.c_str()) == 0;
}


bool Syscaller::linker() {
   std::string call = "ld " + m_obj + " -o " + m_name;
   return system(call.c_str()) == 0;
}


void Syscaller::cleanup() {
   std::string rm = "rm " + m_asm;
   if (!m_opts.keep_asm && !m_opts.debug_enbl) system(rm.c_str());
   rm.clear(); rm = "rm " + m_obj;
   if (!m_opts.keep_obj && !m_opts.debug_enbl) system(rm.c_str());
   rm.clear(); rm = "rm " + m_name + "_preop.asm";
   if (!m_opts.keep_preop && !m_opts.debug_enbl) system(rm.c_str());
}