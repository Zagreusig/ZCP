#include "compiler.h"
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/analyzer/analyer.h"
#include "backend/codegen/generation.h"
#include "backend/optimizations/optimizer.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/ASTPrinter.h"
#include <fstream>
#include <iostream>


int Compiler::run() {
   if (has_flag(Flags::PRINT_FLAGS))
      print_flags(flag_arr);

   Lexer lexer(*this);
   auto tokens = lexer.lex();
   if (has_flag(Flags::PRINT_TOKENS)) print_tokens(tokens);
   if (diag.has_errors()) { diag.report_all(source, filename); return 1; }

   Parser parser(*this, tokens);
   auto prog = parser.parse_prog();
   if (has_flag(Flags::PRINT_AST) && prog) print_ast(*prog);
   if (diag.has_errors()) { diag.report_all(source, filename); return 1; }

   Analyzer analyzer(*this, *prog);
   analyzer.analyze();
   if (diag.has_errors()) { diag.report_all(source, filename); return 1; }

   ASMGenerator gen(*this, *prog);
   m_orig = gen.build();
   if (diag.has_errors()) { diag.report_all(source, filename); return 1; }

   Optimizer optimizer(m_orig);
   optimizer.optimize();
   m_asm_out = optimizer.finish();

   // if (diag.has_errors()) { diag.report_all(source, filename); return 1; }
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


void Compiler::print_tokens(std::vector<Token> toks) {
   for (auto& t : toks) {
      std::cout << "{ " << to_string(t.type);

      if (t.is_text())      std::cout << ", " << t.text();
      else if (t.is_int())  std::cout << ", " << t.int_val();
      else if (t.is_char()) std::cout << ", " << t.char_val();

      std::cout << " " << t.line << ':' << t.col << " }\n";
   }
   std::cout << std::endl;
}


void Compiler::print_flags(std::vector<Flags> flags) {
   for (auto& flag : flags) std::cout << to_str(flag) << " ";
   std::cout << std::endl;
}


void Compiler::print_ast(const NodeProg prog) {
   ASTPrinter p(prog);
   p.print();
}