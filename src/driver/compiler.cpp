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
#include "syscaller.h"
#include "utils/flags.h"
#include "debug/ASTPrinter.h"
#include "Logger.h"
#include "Nodes.h"
#include "TokenTable.h"
#include "Tokens.h"
#include "phase.h"
#include "ErrorHandler.h"


int Compiler::run() {
   if (isFlagPrintingEnabled()) do_flags();

   m_tokens = lex();
   if (isRawTokensEnabled()) do_tokens(m_tokens);
   if (errors(source_text)) return LEX_FAILURE;

   m_tokens = preprocess();
   if (isTokenPrintingEnabled()) do_tokens(m_tokens);
   if (errors(source_text)) return PREPROCESS_FAILURE;

   m_program = parse();
   if (isASTPrintingEnabled() && m_program) do_ast(*m_program);
   if (errors(source_text)) return PARSE_FAILURE;

   analyze();
   if (errors(source_text)) return ANALYSIS_FAILURE;

   m_orig = generate();
   if (logger.enabled()) logger.set_orig_asm(m_orig);
   if (errors(source_text)) return GENERATOR_FAILURE;

   auto returned = optimize();
   m_asm_out = returned.first; optimizer_passes = returned.second;
   
    if (logger.enabled()) do_optimizer_logging(optimizer_passes, m_asm_out);

   write_files();

   syscalls(make_syscall_options());

   if (err_code != 0)
      std::cerr << "ERR: " << (err_code == 1 ? "nasm " : "ld ") << "failed.\n";
   return err_code;
}


std::vector<Token> Compiler::lex() {
   logger.mark_phase(CompPhase::Lexing);
   ScopedPhaseTimer timer(logger, CompPhase::Lexing);
   Lexer lexer(*this, source_text, 0);
   return lexer.lex();
}


std::vector<Token> Compiler::preprocess() {
   logger.mark_phase(CompPhase::Preprocessing);
   ScopedPhaseTimer timer(logger, CompPhase::Preprocessing);
   Preprocessor preprocessor(*this, m_tokens);
   return preprocessor.process();
}


std::optional<NodeProg> Compiler::parse() {
   logger.mark_phase(CompPhase::Parsing);
   ScopedPhaseTimer timer(logger, CompPhase::Parsing);
   Parser parser(*this, m_tokens);
   return parser.parse_prog();
}


void Compiler::analyze() {
   logger.mark_phase(CompPhase::Analysis);
   ScopedPhaseTimer t(logger, CompPhase::Analysis);
   Analyzer analyzer(*this, *m_program);
   analyzer.analyze();
}


std::string Compiler::generate() {
   logger.mark_phase(CompPhase::CodeGen);
   ScopedPhaseTimer t(logger, CompPhase::CodeGen);
   ASMGenerator generator(*this, *m_program);
   return generator.build();
}


std::pair<std::string, int> Compiler::optimize() {
   logger.mark_phase(CompPhase::Optimization);
   ScopedPhaseTimer timer(logger, CompPhase::Optimization);
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
   if (logger.enabled()) logger.set_flags(ss.str());
}


void Compiler::do_tokens(const std::vector<Token>& tokens) {
   std::ostringstream ss;
   print_tokens(ss, tokens);
   if (compiler_opts.toks) std::cout << ss.str();
   if (logger.enabled()) {
      if (compiler_opts.raw) logger.set_raw(ss.str());
      logger.set_tokens(ss.str());
   }
}


void Compiler::do_optimizer_logging(int passes, const std::string& source) {
   logger.record_passes(passes);
   logger.set_opt_asm(source);

   std::fstream _logger("compilation_log.txt", std::ios::out);
   logger.flush(_logger);
}


void Compiler::do_ast(NodeProg program) {
   std::ostringstream ss;
   print_ast(ss, program);
   if (compiler_opts.ast) std::cout << ss.str();
   if (logger.enabled()) logger.set_ast(ss.str());
}


Syscaller::Options Compiler::make_syscall_options() {
   return Syscaller::Options {
      .keep_asm   = has_flag(Flags::LEAVE_ASM),
      .keep_obj   = has_flag(Flags::LEAVE_OBJ),
      .keep_preop = has_flag(Flags::PRESERVE_PRE_OP),
      .debug_enbl = logger.enabled()
   };
}


void Compiler::fatal(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.fatal(phase, file, line, col, msg);
}


void Compiler::error(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.error(phase, file, line, col, msg);
}


void Compiler::warn(CompPhase phase, int fileId, int line, int col, const std::string& msg) {
   std::string file = filename_by_id(fileId);
   diagnostics.warn(phase, file, line, col, msg);
}