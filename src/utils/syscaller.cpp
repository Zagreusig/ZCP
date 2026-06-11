#include "flags.h"
#include "syscaller.h"
#include <fstream>

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

void Syscaller::make_calls() {
   if (flagz) print_flags();
   if (toks)  print_toks();
   if (ast)   print_ast();

   nasm();
   linker();
   cleanup();
}

void Syscaller::nasm() {
   std::string call = "nasm -felf64 ";
   call.append(prog_name + ".asm");

   system(call.c_str());
}


void Syscaller::linker() {
   std::string call = "ld -o ";
   call.append(prog_name + " " + prog_name + ".o");

   system(call.c_str());
}


void Syscaller::cleanup() {
   if (!assm) system("rm *.asm");
   if (!obj)  system("rm *.o");
}