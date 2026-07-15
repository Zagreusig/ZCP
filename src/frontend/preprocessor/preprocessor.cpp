#include "preprocessor.h"

#include <vector>
#include <algorithm>

#include "frontend/lexer/lexer.h"
#include "driver/compiler.h"
#include "utils/file_util.h"
#include "Core/ErrorHandler.h"
#include "Core/TokenTable.h"
#include "Core/Tokens.h"
#include "utils/phase.h"

/** while the line is the same, no specific dermination.
 * #pragma once
 * #
 * pragma (identifier) <- is valid drctv?
 * once   (identifier) <- is valid cmd?
 */

std::vector<Token> Preprocessor::process() {
   std::vector<Token> out;
   while (m_index < m_tokens.size()) {
      if (at_directive()) handle_directive(out);
      else out.push_back(m_tokens[m_index++]);
   }
   return out;
}


bool Preprocessor::at_directive() const {
   const Token& t = m_tokens[m_index];
   return t.type == TokenType::POUND && t.col == 1;
}


void Preprocessor::handle_include(std::vector<Token>& out, int dir_line) {

   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), dir_line, 1, "Expected file path after #include.");
      return;
   }

   const Token& path_token = m_tokens[m_index];
   if (path_token.type != TokenType::STR_LIT) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), path_token.line, path_token.col,
                              "Expected \"path\" after #include.");
      return;
   }
   std::string path = path_token.text();
   m_index++;

   if (m_included.count(path)) return;

   if (std::find(m_include_stack.begin(), m_include_stack.end(), path) != m_include_stack.end()) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), path_token.line, path_token.col,
                              "Circular include of \"" + path + "\".");
      return;
   }

   auto source = read_file(path);
   if (!source) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), path_token.line, path_token.col,
                              "Cannot open \"" + path + "\".");
      return;
   }

   int fileID = m_compiler.add_file(path, *source);

   m_include_stack.push_back(path);
   m_included.insert(path);

   Lexer lex(m_compiler, *source, fileID);
   std::vector<Token> file_tokens = lex.lex();

   // Check if the included file has includes.
   Preprocessor preprocessor(m_compiler, std::move(file_tokens));
   preprocessor.m_included = m_included;
   preprocessor.m_include_stack = m_include_stack;
   std::vector<Token> expanded = preprocessor.process();
   m_included = preprocessor.m_included;

   m_include_stack.pop_back();

   out.insert(out.end(), expanded.begin(), expanded.end());
}


void Preprocessor::handle_directive(std::vector<Token>& out) {
   int dir_line = m_tokens[m_index].line;
   m_index++; // '#'

   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), dir_line, 1,
                                   "Expected directive after '#'");
      return;
   }

   const Token& name = m_tokens[m_index];
   if (!name.is_text()) { 
      m_compiler.diagnostics.error(m_compiler.current_filename(), name.line, name.col, "directive name must be an identifier");
      return;
   }
   m_index++;

   const std::string& directive = name.text();
   if      (directive == "include") handle_include(out, dir_line);
   else if (directive == "pragma") handle_pragma(dir_line);
   else {
      m_compiler.diagnostics.error(m_compiler.current_filename(), name.line, name.col, 
                                   "Unknown directive '#" + directive + "'.");
      skip_to_next_line(dir_line); // recovery: eat the rest of the line :]
   }
}


void Preprocessor::handle_pragma(int dir_line) {
   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.diagnostics.error(CompPhase::Preprocessing, m_compiler.current_filename(), dir_line, 1,
                                   "Expected pragma name.");
      return;
   }

   const Token& name = consume();
   if (name.is_text() && name.text() == "once") return; // no-op: default behavior

   m_compiler.diagnostics.warn(CompPhase::Preprocessing, m_compiler.current_filename(), name.line, name.col,
                               "Unknown pragma directive.");
}


std::vector<Token> Preprocessor::lex_file(const std::string& path, int fileID) {
   std::optional<std::string> source = read_file(path);
   if (!source.has_value()) { return {}; }
   Lexer lex(m_compiler, *source, fileID);
   return lex.lex();
}