#ifndef COMPILER_H
#define COMPILER_H

#include "Core/Nodes.h"
#include "Core/arena.h"
#include "Core/ErrorHandler.h"
#include "utils/flags.h"
#include "debug/Logger.h"
#include <string>
#include <vector>

struct Options {
   bool log;
   bool flags;
   bool toks;
   bool ast;
};

class Compiler {
public:
   std::string filename;
   std::string prog_name;
   std::string source;
   std::vector<Flags> flag_arr;
   ArenaAllocator arena;
   Diagnostics diag;

   Flags _flags = Flags::NONE;
   std::string m_asm_out;
   std::string m_orig;

   Options opts;
   Logger  log;

   Compiler(std::string src, std::vector<Flags> arr, 
            std::string pnm = "out", std::string fname = "")
      : filename(fname), prog_name(pnm), source(src), flag_arr(arr), arena(1024 * 1024 * 4) {
         for (auto& flag : arr) _flags = flag | _flags;
         opts.log = has_flag(Flags::DEBUG); set_bools();
         if (opts.log) log.enable(true);
         diag.attach_logger(&log);
      }


   std::string get_src()      { return source; }
   std::string get_filename() { return filename; }  
   Flags get_flags()          { return _flags; }
   std::vector<Flags> get_flag_vector() { return flag_arr; }

   bool has_flag(Flags f) {
      return static_cast<uint32_t>(static_cast<uint32_t>(_flags) & static_cast<uint32_t>(f));
   }

   void set_bools() {
      if (has_flag(Flags::PRINT_FLAGS)) opts.flags = true;
      if (has_flag(Flags::PRINT_AST)) opts.ast = true;
      if (has_flag(Flags::PRINT_TOKENS)) opts.toks = true;
   }

   // driver func
   int run();

   bool errors(const std::string& source, const std::string& filename) { 
      if (diag.has_errors()) { 
         diag.report_all(source, filename); 
         log.flush_to_file("compilation_log.txt"); 
         return true; 
      } 
      return false; 
   }

   void print_tokens(std::ostringstream&, std::vector<Token>);
   void print_ast(std::ostringstream&, const NodeProg);
   void print_flags(std::ostringstream&, std::vector<Flags>);
};

#endif // COMPILER_H