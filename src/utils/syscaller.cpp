#include "flags.h"
#include "syscaller.h"
#include <fstream>
#include <iostream>

void Syscaller::set_bools() {
   for (auto& flag : flags) {
      switch (flag) {
         case Flags::LEAVE_ASM:    assm  = true; break;
         case Flags::LEAVE_OBJ:    obj   = true; break;
         case Flags::PRINT_TOKENS: toks  = true; break;
         case Flags::PRINT_FLAGS:  flagz = true; break;
         case Flags::PRINT_AST:    ast   = true; break;
         case Flags::USER_NAME:    name  = true; break;
         default:                                break;
      }
   }
}

void Syscaller::make_calls(bool errors) {
   if (flagz) print_flags();
   if (toks)  print_toks();
   if (ast && !errors)   print_ast();

   if (!errors){
      nasm();
      linker();
      cleanup();
   }
}

void Syscaller::nasm() {
   std::string call = "nasm -felf64 ";
   call.append(asm_file);

   system(call.c_str());
}


void Syscaller::linker() {
   std::string call = "ld ";
   call.append(obj_file + " -o " + prog_name);

   system(call.c_str());
}


void Syscaller::cleanup() {
   std::string rm_statement = "rm " + asm_file;
   if (!assm) system(rm_statement.c_str());
   rm_statement.clear(); rm_statement = "rm " + obj_file;
   if (!obj)  system(rm_statement.c_str());
}