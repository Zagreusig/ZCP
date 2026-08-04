#ifndef COMPILER_H
#define COMPILER_H

#include <stdint.h>
#include <stddef.h>
#include <optional>
#include <string>
#include <vector>
#include <iosfwd>
#include <unordered_map>
#include <utility>

#include "Core/Nodes.h"
#include "Core/arena.h"
#include "Core/ErrorHandler.h"
#include "Core/IRDefs.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/Logger.h"
#include "Tokens.h"

struct NodeProg;
struct Token;
enum class CompPhase;

struct Options {
   bool log   = false;
   bool flags = false;
   bool toks  = false;
   bool ast   = false;
   bool raw   = false;
   bool ir    = false;
};

class Compiler {
public:
   std::string        file_name;
   std::string        prog_name;
   std::string        source_text;
   std::vector<Flags> flag_arr;
   ArenaAllocator     allocator;
   Diagnostics        diagnostics;

   Flags              _flags = Flags::NONE;
   std::string        m_asm_out;
   std::string        m_orig;

   std::string        m_macros;

   std::vector<Token>      m_tokens;
   std::optional<NodeProg> m_program;
   int                     optimizer_passes = 0;

   Options compiler_opts;
   Logger  m_logger;

   int main_file_ID = 0, current_file_ID = 0;
   std::unordered_map<int, std::pair<std::string, std::string>> m_files;

   int err_code = 0;

   Compiler(std::string src, std::vector<Flags> arr, 
            std::string pnm = "out", std::string fname = "test")
      : file_name(fname), prog_name(pnm), source_text(src), flag_arr(arr), allocator(1024 * 1024 * 4) {
         for (auto& flag : arr) _flags = flag | _flags;
         compiler_opts.log = has_flag(Flags::DEBUG); set_bools();
         if (compiler_opts.log) m_logger.enable(true);
         diagnostics.attach_logger(&m_logger);
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
   Flags       get_flags()              { return _flags; }
   std::vector<Flags> get_flag_vector() { return flag_arr; }

   bool has_flag(Flags f) {
      return static_cast<uint32_t>(static_cast<uint32_t>(_flags) & static_cast<uint32_t>(f));
   }

   void set_bools() {
      compiler_opts.raw   = has_flag(Flags::RAW_TOKENS);
      compiler_opts.flags = has_flag(Flags::PRINT_FLAGS);
      compiler_opts.ast   = has_flag(Flags::PRINT_AST);
      compiler_opts.toks  = has_flag(Flags::PRINT_TOKENS);
      compiler_opts.ir    = has_flag(Flags::USE_IR);
   }

   bool isRawTokensEnabled()     { return compiler_opts.log || compiler_opts.raw;   }
   bool isFlagPrintingEnabled()  { return compiler_opts.log || compiler_opts.flags; }
   bool isTokenPrintingEnabled() { return compiler_opts.log || compiler_opts.toks;  }
   bool isASTPrintingEnabled()   { return compiler_opts.log || compiler_opts.ast;   }
   bool isLoggingEnabled()       { return compiler_opts.log; }

   // driver func
   int run();

   // phases / steps
   std::vector<Token>          lex();
   std::vector<Token>          preprocess();
   std::optional<NodeProg>     parse();
   void                        analyze();
   IRModule                    IR();
   std::string                 generate();
   std::pair<std::string, int> optimize(); // <optimised asm, number of passes>
   int                         write_files();
   void                        syscalls(Syscaller::Options);

   Syscaller::Options          make_syscall_options();


   bool errors(const std::string& source) {
      if (!m_logger.has_errors()) return false;
      // Logger is the single source of truth (both Diagnostics-routed
      // analyzer errors and Log::-routed lexer/parser errors land there).
      // Diagnostics keeps its own maps only for report_all()'s caret
      // rendering, so only render through it when it actually has entries.
      if (diagnostics.has_errors()) diagnostics.report_all(source);
      m_logger.flush_to_file("compilation_log.txt");
      return true;
   }

   void do_flags();
   void do_tokens(const std::vector<Token>&);
   void do_ast(NodeProg);
   void do_optimizer_logging(int, const std::string&);
   void do_preprocess(const std::string&);

   // Formatting itself lives in debug/TokenPrinter.h / debug/ASTPrinter.h;
   // these do_* methods just decide whether/where to print based on
   // compiler_opts and m_logger state.
   std::string format_tokens(std::vector<Token>& tokens);

   void fatal(CompPhase, int, int, int, const std::string&);
   void error(CompPhase, int, int, int, const std::string&);
   void warn (CompPhase, int, int, int, const std::string&);
};


enum {
   SUCCESS    = 0,
   FILE_ERROR = 8 // not phase-indexed; see exit_code(CompPhase) in utils/phase.h for phase failures
};

#endif // COMPILER_H