#include "compiler.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

#include "frontend/lexer/lexer.h"
#include "frontend/preprocessor/preprocessor.h"
#include "frontend/parser/parser.h"
#include "frontend/analyzer/analyer.h"
#include "backend/codegen/generation.h"
#include "backend/optimizations/optimizer.h"
#include "IR/Lowerer.h"
#include "Core/IRDefs.h"
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/ASTPrinter.h"
#include "debug/IRDebug.h"
#include "debug/Log.h"
#include "Logger.h"
#include "Nodes.h"
#include "TokenTable.h"
#include "Tokens.h"
#include "phase.h"
#include "ErrorHandler.h"


int Compiler::run() {
   Log::set_sink(&m_logger);
   struct SinkGuard { ~SinkGuard() { Log::set_sink(nullptr); } } guard; // clears on throw
   if (isFlagPrintingEnabled()) do_flags();

   m_tokens = lex();
   if (isRawTokensEnabled()) {
      std::string temp = "Lexed " + m_tokens.size();
      Log::info(CompPhase::Lexing, temp + " tokens");
      do_tokens(m_tokens);
   }
   if (errors(source_text)) return LEX_FAILURE;

   m_tokens = preprocess();
   if (isTokenPrintingEnabled()) {
      std::string temp = "Preprocessor token vector size: " + m_tokens.size();
      Log::info(CompPhase::Preprocessing, temp + " tokens");
      do_tokens(m_tokens);
   }
   if (errors(source_text)) return PREPROCESS_FAILURE;

   m_program = parse();
   if (isASTPrintingEnabled() && m_program) do_ast(*m_program);
   if (errors(source_text)) return PARSE_FAILURE;

   analyze();
   if (errors(source_text)) return ANALYSIS_FAILURE;

   Lowerer ir;
   IRModule mod = ir.lower(*m_program);

   IRPrinter printer(std::cout);
   printer.print(mod);

   m_orig = generate();
   if (m_logger.enabled()) m_logger.set_orig_asm(m_orig);
   if (errors(source_text)) return GENERATOR_FAILURE;

   auto returned = optimize();
   m_asm_out = returned.first; optimizer_passes = returned.second;
   
   if (m_logger.enabled()) {
      do_optimizer_logging(optimizer_passes, m_asm_out);
   }

   write_files();

   syscalls(make_syscall_options());

   if (err_code != 0)
      std::cerr << "ERR: " << (err_code == 1 ? "nasm " : "ld ") << "failed.\n";
   return err_code;
}


std::vector<Token> Compiler::lex() {
   m_logger.mark_phase(CompPhase::Lexing);
   ScopedPhaseTimer timer(m_logger, CompPhase::Lexing);
   Lexer lexer(*this, source_text, 0);
   return lexer.lex();
}


/** TODO: Figure a way to get this out */
std::vector<Token> Compiler::preprocess() {
   m_logger.mark_phase(CompPhase::Preprocessing);
   Preprocessor preprocessor(*this, m_tokens);
   std::vector<Token> temp;
   {
      ScopedPhaseTimer timer(m_logger, CompPhase::Preprocessing);
      temp = preprocessor.process();
   }
   if (isLoggingEnabled()) do_preprocess(preprocessor.dump());
   return temp;
}


std::optional<NodeProg> Compiler::parse() {
   m_logger.mark_phase(CompPhase::Parsing);
   ScopedPhaseTimer timer(m_logger, CompPhase::Parsing);
   Parser parser(*this, m_tokens);
   return parser.parse_prog();
}


void Compiler::analyze() {
   m_logger.mark_phase(CompPhase::Analysis);
   ScopedPhaseTimer t(m_logger, CompPhase::Analysis);
   Analyzer analyzer(*this, *m_program);
   analyzer.analyze();
}


std::string Compiler::generate() {
   m_logger.mark_phase(CompPhase::CodeGen);
   ScopedPhaseTimer t(m_logger, CompPhase::CodeGen);
   ASMGenerator generator(*this, *m_program);
   return generator.build();
}


std::pair<std::string, int> Compiler::optimize() {
   m_logger.mark_phase(CompPhase::Optimization);
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


void Compiler::print_tokens(std::vector<Token> tokens) {
   std::ostringstream ss;
   print_tokens(ss, tokens);
   std::fstream file("raw_tokens.txt", std::ios::out);
   if (!file.is_open()) return;
   file << ss.str();
}


std::string Compiler::format_tokens(std::vector<Token>& tokens) {
   std::ostringstream ss;
   print_tokens(ss, tokens);
   return ss.str();
}


std::string Compiler::format_raw(const std::vector<Token>& tokens) {
   std::ostringstream ss;
   for (auto& token : tokens)
      ss << token.name() << ", ";
   ss << "\n";
   return ss.str();
}


void Compiler::print_tokens(std::ostringstream& ss, std::vector<Token> tokens) {
   for (auto& token : tokens) {
      ss << "{ " << to_string(token.type);
      if     (token.is_text())  ss << ", " << token.text();
      else if (token.is_int())  ss << ", " << token.int_val();
      else if (token.is_char()) ss << ", " << token.char_val();
      else if (token.is_bool()) ss << ", " << token.bool_val();
   
      ss <<  ", " << filename_by_id(token.fileId) << ":" 
                 << token.line << ':' << token.col << " }\n";
   }
   ss << std::endl;
} 


void Compiler::print_flags(std::ostringstream& ss, std::vector<Flags> flags) {
   for (auto& flag : flags) ss << to_str(flag) << " ";
   ss << std::endl;
}


void Compiler::print_ast(std::ostringstream& ss, const NodeProg prog) {
   ASTPrinter printer(prog, ss);
   printer.print();
}


void Compiler::do_flags() {
   std::ostringstream ss;
   print_flags(ss, flag_arr);
   if (compiler_opts.flags) std::cout << ss.str();
   if (m_logger.enabled()) m_logger.set_flags(ss.str());
}


void Compiler::do_tokens(const std::vector<Token>& tokens) {
   std::ostringstream ss;
   print_tokens(ss, tokens);
   if (compiler_opts.toks) std::cout << ss.str();
   if (m_logger.enabled()) {
      if (compiler_opts.raw) m_logger.set_raw(ss.str());
      m_logger.set_tokens(ss.str());
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
   print_ast(ss, program);
   if (compiler_opts.ast) std::cout << ss.str();
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
   Log::error(phase, msg, file, line, col);
   diagnostics.error(phase, file, line, col, msg);
}


void Compiler::warn(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   Log::warn(phase, msg, file, line, col);
   diagnostics.warn(phase, file, line, col, msg);
}