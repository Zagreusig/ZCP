#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <optional>
#include <string>
#include <vector>
#include <iosfwd>
#include <unordered_map>
#include <utility>

#include "Core/Nodes.h"
#include "Core/arena.h"
#include "Core/ErrorHandler.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/Logger.h"
#include "Tokens.h"

struct NodeProg;
struct Token;

struct Options {
   bool log;
   bool flags;
   bool toks;
   bool ast;
   bool raw;
};

class Compiler {
public:
   std::string file_name;
   std::string prog_name;
   std::string source_text;
   std::vector<Flags> flag_arr;
   ArenaAllocator allocator;
   Diagnostics diagnostics;

   Flags _flags = Flags::NONE;
   std::string m_asm_out;
   std::string m_orig;

   std::vector<Token>      m_tokens;
   std::optional<NodeProg> m_program;
   int optimizer_passes = 0;

   Options compiler_opts;
   Logger  log;

   int main_file_ID = 0, current_file_ID = 0;
   std::unordered_map<int, std::pair<std::string, std::string>> m_files;

   int err_code = 0;

   Compiler(std::string src, std::vector<Flags> arr, 
            std::string pnm = "out", std::string fname = "test")
      : file_name(fname), prog_name(pnm), source_text(src), flag_arr(arr), allocator(1024 * 1024 * 4) {
         for (auto& flag : arr) _flags = flag | _flags;
         compiler_opts.log = has_flag(Flags::DEBUG); set_bools();
         if (compiler_opts.log) log.enable(true);
         diagnostics.attach_logger(&log);
         main_file_ID = add_file(file_name, source_text);
      }

   int add_file(const std::string& path, const std::string& source) {
      m_files[m_files.size()] = { path, source };
      return current_file_ID = (int)m_files.size() - 1;
   }

   const std::string filename_by_id(size_t id) const { return m_files.size() > 0 && id <= m_files.size() ? m_files.at(id).first : "Null"; }

   std::string get_src()                { return source_text; }
   std::string get_filename()           { return file_name; }  
   std::string current_filename()       { return m_files.size() > 0 ? m_files.at(current_file_ID).first : "Null"; }
   int         current_fileID()         { return current_file_ID; }
   Flags get_flags()                    { return _flags; }
   std::vector<Flags> get_flag_vector() { return flag_arr; }

   bool has_flag(Flags f) {
      return static_cast<uint32_t>(static_cast<uint32_t>(_flags) & static_cast<uint32_t>(f));
   }

   void set_bools() {
      compiler_opts.raw   = has_flag(Flags::RAW_TOKENS);
      compiler_opts.flags = has_flag(Flags::PRINT_FLAGS);
      compiler_opts.ast   = has_flag(Flags::PRINT_AST);
      compiler_opts.toks  = has_flag(Flags::PRINT_TOKENS);
   }

   bool isRawTokensEnabled()     { return compiler_opts.log || compiler_opts.raw;   }
   bool isFlagPrintingEnabled()  { return compiler_opts.log || compiler_opts.flags; }
   bool isTokenPrintingEnabled() { return compiler_opts.log || compiler_opts.toks;  }
   bool isASTPrintingEnabled()   { return compiler_opts.log || compiler_opts.ast;   }

   // driver func
   int run();

   // phases / steps
   std::vector<Token>          lex();
   std::vector<Token>          preprocess();
   std::optional<NodeProg>     parse();
   void                        analyze();
   std::string                 generate();
   std::pair<std::string, int> optimize(); // <optimised asm, number of passes>
   int                         write_files();
   void                        syscalls(Syscaller::Options);

   Syscaller::Options          make_syscall_options();


   bool errors(const std::string& source, const std::string& filename) { 
      if (diagnostics.has_errors()) { 
         diagnostics.report_all(source, filename); 
         log.flush_to_file("compilation_log.txt"); 
         return true; 
      } 
      return false; 
   }

   void do_flags();
   void do_tokens(const std::vector<Token>&);
   void do_ast(NodeProg);
   void do_optimizer_logging(int, const std::string&);

   void print_tokens(std::vector<Token>);
   void print_tokens(std::ostringstream&, std::vector<Token>);
   void print_ast(std::ostringstream&, const NodeProg);
   void print_flags(std::ostringstream&, std::vector<Flags>);
};


enum {
   SUCCESS            = 0,
   LEX_FAILURE        = 1,
   PREPROCESS_FAILURE = 2,
   PARSE_FAILURE      = 3,
   ANALYSIS_FAILURE   = 4,
   GENERATOR_FAILURE  = 5,
   OPTIMIZER_FAILURE  = 6,
   SYSCALLER_FAILURE  = 7,
   FILE_ERROR         = 8
};

#endif // COMPILER_H