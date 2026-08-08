#include "preprocessor.h"

#include <vector>
#include <algorithm>
#include <sstream>

#include "frontend/lexer/lexer.h"
#include "driver/compiler.h"
#include "utils/file_util.h"
#include "Core/TokenTable.h"
#include "Core/Tokens.h"

std::vector<Token> Preprocessor::process() {
   std::vector<Token> out; out.reserve(500);
   std::set<std::string> active;
   while (m_index < m_tokens.size()) {
      if (at_directive()) {
         handle_directive(out); continue;
      }
      if (try_expand(m_tokens, m_index, out, active, 0)) continue;
      out.push_back(consume());     
   }
   return out;
}


bool Preprocessor::at_directive() const {
   const Token& token = m_tokens[m_index];
   return token.type == TokenType::POUND && token.col == 1;
}


const Macro* Preprocessor::get_macro(const Token& token) {
   if (token.type != TokenType::IDENTIFIER) return nullptr;
   auto iterator = m_macros.find(token.text());
   return iterator != m_macros.end() ? &iterator->second : nullptr;
}


void Preprocessor::expand_into(const std::vector<Token>& input, 
                               std::vector<Token>& out, 
                               std::set<std::string>& active,
                               int depth) {
   for (size_t i = 0; i < input.size(); ) {
      if (try_expand(input, i, out, active, depth)) continue;
      out.push_back(input[i]); i++;
   }
}


void Preprocessor::handle_include(std::vector<Token>& out, int dir_line) {

   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.error(m_compiler.current_file_ID, dir_line, 1, "Expected file path after #include.");
      return;
   }

   const Token& path_token = consume();
   if (path_token.type != TokenType::STR_LIT) {
      m_compiler.error(path_token.fileId, path_token.line, path_token.col,
                              "Expected \"path\" after #include.");
      return;
   }
   std::string path = path_token.text();

   if (m_included.count(path)) return;

   if (std::find(m_include_stack.begin(), m_include_stack.end(), path) != m_include_stack.end()) {
      m_compiler.error(path_token.fileId, path_token.line, path_token.col,
                              "Circular include of \"" + path + "\".");
      return;
   }

   auto source = read_file(path);
   if (!source) {
      m_compiler.error(path_token.fileId, path_token.line, path_token.col,
                              "Cannot open \"" + path + "\".");
      return;
   }

   int fileID = m_compiler.add_file(path, *source);

   m_include_stack.push_back(path);
   m_included.insert(path);

   Lexer lex(*source, m_file_name, fileID);
   std::vector<Token> file_tokens = lex.lex();

   // Check if the included file has includes.
   Preprocessor preprocessor(m_compiler, std::move(file_tokens), m_file_name);
   preprocessor.m_included = m_included;
   preprocessor.m_include_stack = m_include_stack;
   std::vector<Token> expanded = preprocessor.process();
   m_included = preprocessor.m_included;

   m_include_stack.pop_back();

   out.insert(out.end(), expanded.begin(), expanded.end());
}


void Preprocessor::handle_directive(std::vector<Token>& out) {
   int dir_line = m_tokens[m_index].line;
   consume(); // '#'

   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.error(m_compiler.current_file_ID, dir_line, 1,
                       "Expected directive after '#'");
      return;
   }

   const Token& name = consume();
   if (!name.is_text()) { 
      m_compiler.error(name.fileId, name.line, name.col, "directive name must be an identifier");
      return;
   }

   const std::string& directive = name.text();
   if      (directive == "include") handle_include(out, dir_line);
   else if (directive == "define")  handle_define(dir_line);
   else if (directive == "pragma")  handle_pragma(dir_line);
   else {
      m_compiler.error(name.fileId, name.line, name.col, 
                       "Unknown directive '#" + directive + "'.");
      skip_to_next_line(dir_line); // recovery: eat the rest of the line :]
   }
}


void Preprocessor::handle_pragma(int dir_line) {
   if (m_index >= m_tokens.size() || m_tokens[m_index].line != dir_line) {
      m_compiler.error(m_compiler.current_file_ID, dir_line, 1,
                       "Expected pragma name.");
      return;
   }

   const Token& name = consume();
   if (name.is_text() && name.text() == "once") return; // no-op: default behavior

   m_compiler.warn(name.fileId, name.line, name.col,
                   "Unknown pragma directive.");
}


void Preprocessor::handle_define(int dir_line) {
   if (!peek().has_value() || peek().value().line != dir_line) {
      int fid = 0, line = 0, col = 0;
      if (peek().has_value()) { fid = peek().value().fileId; line = peek().value().line; col = peek().value().col; }
      m_compiler.error(fid, line, col, "Expected macro name after #define."); return;
   }

   const Token& name_token = consume();
   if (!name_token.is_text()) { m_compiler.error(name_token.fileId, 
                                                 name_token.line, name_token.col, "Macro name must be an identifier."); return;} 
   
   Macro macro;
   macro.name = name_token.text(); macro.origin_file = name_token.fileId; macro.line = name_token.line;

   const Token& next_token = m_tokens[m_index];
   macro.is_function_like = (next_token.type == TokenType::OPEN_PAREN && next_token.line == name_token.line &&
                             next_token.col == name_token.col + (int)name_token.text().size());

   if (macro.is_function_like) {
      consume(); // (
      while (m_index < m_tokens.size() && m_tokens[m_index].type != TokenType::CLOSE_PAREN) {
         const Token& parameter = consume();
         if (parameter.type == TokenType::IDENTIFIER)
            macro.params.push_back(parameter.text());
         if (m_tokens[m_index].type == TokenType::COMMA) consume();
      }
      consume(); // )
   }

   // collecting the macro :]
   while (m_index < m_tokens.size() && m_tokens[m_index].line == dir_line)
      macro.content.push_back(consume());
   m_macros[macro.name] = macro;
}


std::vector<Token> Preprocessor::lex_file(const std::string& path, int fileID) {
   std::optional<std::string> source = read_file(path);
   if (!source.has_value()) { return {}; }
   Lexer lex(*source, m_file_name, fileID);
   return lex.lex();
}


std::vector<std::vector<Token>> Preprocessor::collect_args(const std::vector<Token>& tokens, size_t& position) {
   position++;
   std::vector<std::vector<Token>> args;
   std::vector<Token> current;
   int depth = 0;

   while (position < tokens.size()) {
      const Token& token = tokens[position++];
      if (token.type == TokenType::OPEN_PAREN) { depth++; current.push_back(token); }
      else if (token.type == TokenType::CLOSE_PAREN) {
         if (depth == 0) break;
         depth--; current.push_back(token);
      }
      else if (token.type == TokenType::COMMA && depth == 0) {
         args.push_back(current); current.clear();
      }
      else current.push_back(token);
   }

   if (!current.empty()) args.push_back(current);
   return args;
}


std::vector<Token> Preprocessor::substitute(const Macro& macro, const std::vector<std::vector<Token>>& args) {
   std::vector<Token> result;
   for (const Token& token : macro.content) {
      int param_index = -1;
      if (token.type == TokenType::IDENTIFIER)
         for (size_t i = 0; i < macro.params.size(); i++)
            if (macro.params[i] == token.text()) { param_index = (int)i; break; }
      
      if (param_index >= 0)
         for (const Token& arg_token : args[param_index])
            result.push_back(arg_token);
      else
         result.push_back(token);
   }
   return result;
}


bool Preprocessor::try_expand(const std::vector<Token>& tokens, size_t& position, 
                              std::vector<Token>& out, std::set<std::string>& active, int depth) {
   const Token& token = tokens[position];
   if (token.type != TokenType::IDENTIFIER) return false;

   auto iterator = m_macros.find(token.text());
   if (iterator == m_macros.end() || active.count(token.text()) != 0) return false;

   const Macro& macro = iterator->second;

   if (!macro.is_function_like) {
      position++;
      active.insert(macro.name);
      expand_into(macro.content, out, active, depth + 1);
      active.erase(macro.name);
      return true;
   }

   if (position + 1 < tokens.size() && tokens[position + 1].type == TokenType::OPEN_PAREN) {
      size_t pos = position + 1;
      auto args = collect_args(tokens, pos);
      if (args.size() != macro.params.size()) {
         m_compiler.error(token.fileId, token.line, token.col,
                          "Macro argument count mismatch.");
         position = pos;
         return true;
      }

      auto substituted = substitute(macro, args);
      active.insert(macro.name);
      expand_into(substituted, out, active, depth + 1);
      active.erase(macro.name);
      position = pos;
      return true;
   }

   return false;
}


std::string Preprocessor::dump() {
   std::ostringstream ss;
   ss << "Number of macros: " << m_macros.size() << "\n";

   for (const auto& macro : m_macros) {
      ss << macro.first << ": param size: " << macro.second.params.size() << ", vector size: " << macro.second.content.size() << "\n";
      ss << "-----------------------------------------\n\n";
      std::vector<Token> cont = macro.second.content;
      ss << "Params:\n";
      for (const auto& param : macro.second.params) {
         ss << param << std::endl;
      }
      ss << "\nContent:\n";
      ss << m_compiler.format_tokens(cont) << "\n\n";
   }

   return ss.str();
}