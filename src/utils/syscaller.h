#ifndef SYSCALLER_H
#define SYSCALLER_H

#include <iostream>
#include <stdint.h>
#include <string>
#include <sstream>
#include <vector>
#include "ast/ASTPrinter.h"
#include "flags.h"
#include "lexer/Tokens.h"
#include "parser/parser.h"

class Syscaller {
public:
   inline Syscaller(NodeProg prog) : printer(prog) {}
   inline Syscaller(std::string name, std::vector<Token> tks, std::vector<Flags> fgs, NodeProg prog)
      : flags(std::move(fgs)), tokens(tks), printer(prog) { 
         all_flags = combine_flags();
         if (name != "out") {
            prog_name.erase();
            prog_name = name;  
         }
         asm_file = prog_name + ".asm";
         obj_file = prog_name + ".o";
         set_bools(); 
      }

   bool has_flag(Flags f) {
      return static_cast<uint32_t>(all_flags & static_cast<uint32_t>(f));
   }
  
   std::string get_name() { return prog_name; }

   void set_name(std::string name) { 
      if (name != "out") {
         prog_name.erase();
         prog_name = name;
      }
   }

   void set_flags(std::vector<Flags> fgs) { flags = std::move(fgs); }
   void give_toks(std::vector<Token> tks) { tokens = std::move(tks); }

   void set_bools();
   void make_calls();
private:
   uint32_t combine_flags() {
      Flags temp {};
      for (auto& f : flags) temp = f | temp;
      return static_cast<uint32_t>(temp);
   }

   void print_ast()   { printer.print(); }
   void print_flags() { for (auto& flag : flags) std::cout << to_str(flag) << " ";      std::cout << std::endl; }
   void print_toks()  { for (auto& t : tokens)   std::cout << to_string(t.type) << " "; std::cout << std::endl; }
   void nasm();
   void linker();
   void cleanup();

   ASTPrinter printer;

   std::string prog_name = "out";
   std::string asm_file {};
   std::string obj_file {};
   std::vector<Flags> flags {};
   std::vector<Token> tokens {};
   uint32_t all_flags {};

   bool name {}, assm {}, ast{}, obj {}, flagz {}, toks {};
};

#endif // SYSCALLER_H