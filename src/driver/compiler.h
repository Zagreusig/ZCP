#ifndef COMPILER_H
#define COMPILER_H

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "analyzer/analyer.h"
#include "codegen/generation.h"
#include "optimizations/optimizer.h"
#include "ast/arena.h"
#include "ErrAndRep/ErrorHandler.h"
#include "utils/syscaller.h"
#include "utils/flags.h"
#include <iostream>
#include <string>
#include <vector>

class Compiler {
public:
   std::string filename;
   std::string prog_name;
   std::string source;
   std::vector<Flags> flag_arr;
   ArenaAllocator arena;
   Diagnostics diag;

   Flags _flags;
   std::string m_asm_out;
   std::string m_orig;

   Compiler(std::string src, std::vector<Flags> arr, std::string pnm = "out", std::string fname = "")
      : filename(fname), prog_name(pnm), source(src), flag_arr(arr), arena(1024 * 1024 * 4) {
         Flags t = Flags::NONE;
         for (auto& flag : arr) {
            t = flag | t;
         }
         _flags = t;
      }


   std::string get_src()      { return source; }
   std::string get_filename() { return filename; }  
   Flags get_flags()          { return _flags; }
   std::vector<Flags> get_flag_vector() { return flag_arr; }

   bool has_flag(Flags f) {
      return static_cast<uint32_t>(static_cast<uint32_t>(_flags) & static_cast<uint32_t>(f));
   }

   // driver func
   int run();

   void print_tokens(std::vector<Token>);
   void print_ast(const NodeProg);
   void print_flags(std::vector<Flags>);
};

#endif // COMPILER_H