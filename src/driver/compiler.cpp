#include "compiler.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

#include "frontend/lexer/lexer.h"
#include "frontend/preprocessor/preprocessor.h"
#include "frontend/parser/parser.h"
#include "frontend/analyzer/analyer.h"
#include "backend/codegen/generation.h"
#include "backend/optimizations/optimizer.h"
#include "backend/codegen/backend.h"
#include "IR/Lowerer.h"
#include "Core/IRDefs.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/ASTPrinter.h"
#include "debug/IRDebug.h"
#include "debug/Log.h"
#include "debug/Format.h"
#include "Logger.h"
#include "Nodes.h"
#include "Tokens.h"
#include "phase.h"
#include "ErrorHandler.h"


int Compiler::run() {
   Log::set_sink(&m_logger);
   struct SinkGuard { ~SinkGuard() { Log::set_sink(nullptr); } } guard; // clears on throw
   if (isFlagPrintingEnabled()) do_flags();

   m_tokens = lex();
   if (isRawTokensEnabled()) {
      // std::string temp = "Lexed " + m_tokens.size();
      // Log::info(CompPhase::Lexing, temp + " tokens");
      do_tokens(m_tokens);
   }
   if (errors(source_text)) return exit_code(CompPhase::Lexing);

   m_tokens = preprocess();
   if (isTokenPrintingEnabled()) {
      // std::string temp = "Preprocessor token vector size: " + m_tokens.size();
      // Log::info(CompPhase::Preprocessing, temp + " tokens");
      do_tokens(m_tokens);
   }
   if (errors(source_text)) return exit_code(CompPhase::Preprocessing);

   m_program = parse();
   if (isASTPrintingEnabled() && m_program) do_ast(*m_program);
   if (errors(source_text)) return exit_code(CompPhase::Parsing);

   analyze();
   if (errors(source_text)) return exit_code(CompPhase::Analysis);

   m_logger.mark_phase(CompPhase::Lowering);
   // if (compiler_opts.ir) {
      IRModule mod = IR();

      if (isLoggingEnabled()) {
         std::ostringstream out;
         IRPrinter printer(out);
         printer.print(mod);
         m_logger.set_ir_mod(out.str());
      }

      m_logger.mark_phase(CompPhase::CodeGen);
      {
         ScopedPhaseTimer t(m_logger, CompPhase::CodeGen);
         Backend generator;
         m_orig = generator.generate(mod);
      }
   // } else {
   //    m_orig = generate();
   //    if (m_logger.enabled()) m_logger.set_orig_asm(m_orig);
   //    if (errors(source_text)) return exit_code(CompPhase::CodeGen);
   // }
   // auto returned = optimize();
   // m_asm_out = returned.first; optimizer_passes = returned.second;
   m_asm_out = m_orig; optimizer_passes = 0;
   // if (m_logger.enabled()) {
   //    do_optimizer_logging(optimizer_passes, m_asm_out);
   // }

   // std::fstream comp_log("compilation_log.txt", std::ios::out);
   // if (!comp_log.is_open()) { std::cerr << "Comp log not open." << std::endl; return -99; }

   // m_logger.flush(comp_log);
   // comp_log.close();
   write_files();

   syscalls(make_syscall_options());

   if (err_code != 0)
      std::cerr << "ERR: " << (err_code == 1 ? "nasm " : "ld ") << "failed.\n";
   return err_code;
}


std::vector<Token> Compiler::lex() {
   mark_phase(CompPhase::Lexing);
   ScopedPhaseTimer timer(m_logger, CompPhase::Lexing);
   Lexer lexer(source_text, get_filename(), current_file_ID);
   return lexer.lex();
}


/** TODO: Figure a way to get this out */
std::vector<Token> Compiler::preprocess() {
   mark_phase(CompPhase::Preprocessing);
   Preprocessor preprocessor(*this, m_tokens, get_filename());
   std::vector<Token> temp;
   {
      ScopedPhaseTimer timer(m_logger, CompPhase::Preprocessing);
      temp = preprocessor.process();
   }
   if (isLoggingEnabled()) do_preprocess(preprocessor.dump());
   return temp;
}


std::optional<NodeProg> Compiler::parse() {
   mark_phase(CompPhase::Parsing);
   ScopedPhaseTimer timer(m_logger, CompPhase::Parsing);
   Parser parser(*this, m_tokens, get_filename());
   return parser.parse_prog();
}


void Compiler::analyze() {
   mark_phase(CompPhase::Analysis);
   ScopedPhaseTimer t(m_logger, CompPhase::Analysis);
   Analyzer analyzer(*this, *m_program);
   analyzer.analyze();
}


IRModule Compiler::IR() {
   mark_phase(CompPhase::Lowering);
   ScopedPhaseTimer t(m_logger, CompPhase::Lowering);
   Lowerer lowerer;
   return lowerer.lower(*m_program);
}


std::string Compiler::generate() {
   mark_phase(CompPhase::CodeGen);
   ScopedPhaseTimer t(m_logger, CompPhase::CodeGen);
   ASMGenerator generator(*this, *m_program);
   return generator.build();
}


std::pair<std::string, int> Compiler::optimize() {
   if (compiler_opts.ir || compiler_opts.log) return { m_orig, 0 };
   mark_phase(CompPhase::Optimization);
   ScopedPhaseTimer timer(m_logger, CompPhase::Optimization);
   Optimizer optimizer(m_orig);
   optimizer.optimize();
   return { optimizer.finish(), optimizer.passes() };
}


int Compiler::write_files() {
   std::fstream _asm; _asm.open(prog_name + ".asm", std::ios::out);
   std::fstream _orig; _orig.open(prog_name + "_preop.asm", std::ios::out);
   if (!_asm.is_open())  { std::cerr << "asm file failed to open.\n";     return FILE_ERROR; }
   if (!_orig.is_open()) {std::cerr << "orig.asm file failed to open.\n"; return FILE_ERROR; }

   _asm << m_asm_out;
   _orig << m_orig;
   return SUCCESS;
}


void Compiler::syscalls(Syscaller::Options options) {
   Syscaller syscaller(prog_name, options);
   err_code = syscaller.create_executable();
}


std::string Compiler::format_tokens(std::vector<Token>& tokens) {
   return Format::print(tokens, [this](int id) { return filename_by_id(id); });
   // return TokenPrinter::format(tokens, [this](int id) { return filename_by_id(id); });
}


void Compiler::do_flags() {
   std::string s = Format::print(flag_arr);
   if (compiler_opts.flags) std::cout << s << std::endl;
   if (m_logger.enabled()) m_logger.set_flags(s);
}


void Compiler::do_tokens(const std::vector<Token>& tokens) {
   std::string str = Format::print(tokens, [this](int id) { return filename_by_id(id); });
   if (compiler_opts.toks) std::cout << str << std::endl;
   if (m_logger.enabled()) {
      if (compiler_opts.raw) m_logger.set_raw(str);
      m_logger.set_tokens(str);
   }
}


void Compiler::do_optimizer_logging(int passes, const std::string& source) {
   m_logger.record_passes(passes);
   m_logger.set_opt_asm(source);

   std::fstream _logger("compilation_log.txt", std::ios::out);
   m_logger.flush(_logger);
}


void Compiler::do_ast(NodeProg program) {
   std::ostringstream ss;
   ASTPrinter(program, ss).print();
   if (compiler_opts.ast) std::cout << ss.str() << std::endl;
   if (m_logger.enabled()) m_logger.set_ast(ss.str());
}


void Compiler::do_preprocess(const std::string& str) {
   if (m_logger.enabled()) m_logger.set_macros(str);
}


Syscaller::Options Compiler::make_syscall_options() {
   return Syscaller::Options {
      .keep_asm   = has_flag(Flags::LEAVE_ASM),
      .keep_obj   = has_flag(Flags::LEAVE_OBJ),
      .keep_preop = has_flag(Flags::PRESERVE_PRE_OP),
      .debug_enbl = m_logger.enabled()
   };
}


void Compiler::fatal(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.fatal(phase, file, line, col, msg);
}




void Compiler::error(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.error(file, line, col, msg);
   // diagnostics.error(phase, file, line, col, msg);
}


void Compiler::warn(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.warn(file, line, col, msg);
   // diagnostics.warn(phase, file, line, col, msg);
}


void Compiler::mark_phase(CompPhase phase) {
   m_logger.mark_phase(phase);
   diagnostics.set_phase(phase);
}