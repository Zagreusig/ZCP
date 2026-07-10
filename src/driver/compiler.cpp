#include "compiler.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/analyzer/analyer.h"
#include "backend/codegen/generation.h"
#include "backend/optimizations/optimizer.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/ASTPrinter.h"
#include "Logger.h"
#include "Nodes.h"
#include "TokenTable.h"
#include "Tokens.h"
#include "phase.h"


int Compiler::run() {
   if (opts.log || opts.flags) {
      std::ostringstream ss;
      print_flags(ss, flag_arr);
      if (opts.flags) std::cout << ss.str();
      if (log.enabled()) log.set_flags(ss.str());
   }

   Lexer lexer(*this);
   std::vector<Token> tokens;
   log.mark_phase(CompPhase::Lexing);
   {
      ScopedPhaseTimer t(log, CompPhase::Parsing);
      tokens = lexer.lex();
      if (opts.log || opts.toks) {
         std::ostringstream ss;
         print_tokens(ss, tokens);
         if (opts.toks) std::cout << ss.str();
         if (log.enabled()) log.set_tokens(ss.str());
      }
   }
   if (errors(source, filename)) return 1;


   Parser parser(*this, tokens);
   log.mark_phase(CompPhase::Parsing);

   std::optional<NodeProg> prog;
   {  
      ScopedPhaseTimer t(log, CompPhase::Parsing);
      prog = parser.parse_prog();
   }
   if ((opts.log || opts.ast) && prog) {
      std::ostringstream ss;
      print_ast(ss, *prog);
      if (opts.ast) std::cout << ss.str();
      if (log.enabled()) log.set_ast(ss.str());
   } 
   if (errors(source, filename)) return 1;

   Analyzer analyzer(*this, *prog);
   log.mark_phase(CompPhase::Analysis);
   {   
      ScopedPhaseTimer t(log, CompPhase::Analysis);
      analyzer.analyze();
   }

   if (errors(source, filename)) return 1;

   ASMGenerator gen(*this, *prog);
   log.mark_phase(CompPhase::CodeGen);
   {
      ScopedPhaseTimer t(log, CompPhase::CodeGen);
      m_orig = gen.build();
   }
   if (log.enabled()) log.set_orig_asm(m_orig);
   if (errors(source, filename)) return 1;

   Optimizer optimizer(m_orig);
   log.mark_phase(CompPhase::Optimization);
   {
      ScopedPhaseTimer t(log, CompPhase::Optimization);
      optimizer.optimize();
   }
   m_asm_out = optimizer.finish();

   if (log.enabled()) {
      log.record_passes(optimizer.passes());
      log.set_opt_asm(optimizer.finish());

      std::fstream _log("compilation_log.txt", std::ios::out);
      log.flush(_log);
   }

   {
      std::fstream _asm; _asm.open(prog_name + ".asm", std::ios::out);
      std::fstream _orig; _orig.open(prog_name + "_preop.asm", std::ios::out);
      if (!_asm.is_open()) { std::cerr << "asm file failed to open.\n"; return 1; }
      if (!_orig.is_open()) {std::cerr << "orig.asm file failed to open.\n"; return 1; }

      _asm << m_asm_out;
      _orig << m_orig;
   }

   Syscaller::Options topts {
      .keep_asm   = has_flag(Flags::LEAVE_ASM),
      .keep_obj   = has_flag(Flags::LEAVE_OBJ),
      .keep_preop = has_flag(Flags::PRESERVE_PRE_OP)
   };
   Syscaller sys(prog_name, topts);
   int e = sys.assemble_and_link();
   if (e != 0)
      std::cerr << "ERR: " << (e == 1 ? "nasm " : "ld ") << "failed.\n";
   return e;
}


void Compiler::print_tokens(std::ostringstream& ss, std::vector<Token> toks) {
   for (auto& t : toks) {
      ss << "{ " << to_string(t.type);
      if (t.is_text())      ss << ", " << t.text();
      else if (t.is_int())  ss << ", " << t.int_val();
      else if (t.is_char()) ss << ", " << t.char_val();
   
      ss << " " << t.line << ':' << t.col << " }\n";
   }
   ss << std::endl;
} 


void Compiler::print_flags(std::ostringstream& ss, std::vector<Flags> flags) {
   for (auto& flag : flags) ss << to_str(flag) << " ";
   ss << std::endl;
}


void Compiler::print_ast(std::ostringstream& ss, const NodeProg prog) {
   ASTPrinter p(prog, ss);
   p.print();
}