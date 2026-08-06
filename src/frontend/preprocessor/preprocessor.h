#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

/**
* include import with
* # .. \\
* define(def) undef ifndef
* pragma once
* if elif else fi
* error warning
* # turns macro to string
* ## concats macro toks
* <> "" for files
* <> system, "" use current dir
*/

/**
* Lexer -> preprocessor -> parser ...
* Parse all of the headers for definitions, create a sort of global table of definitions
* unordered map?
*
*
* class Preprocessor
* #define BIT 1
* #define BANG(x) !x
* SYMBOLS = {
*  { "this header": { list, of, definitions, here }},
*  { "preproc.h": { {"Preprocessor", PREPROC::CLASS, <obj>}, {"BIT", PREPROC::MACRO, <obj>}, { "BANG", PREPROC::MACRO, <obj>}}}
* }
*
*
* IFNDEF THIS_H <- macro
* Find the matching .z file, 
*/

#include <stddef.h>
#include <set>
#include <string>
#include <optional>
#include <unordered_map>
#include <vector>
#include <utility>

#include "Core/Tokens.h"

class Compiler;
enum class TokenType;

struct Macro {
   std::string name;
   std::vector<std::string> params;
   bool is_function_like = false;
   std::vector<Token> content;
   int origin_file = 0, line = 0;
};

class Preprocessor {
public:
   Preprocessor(Compiler& cmp, std::vector<Token> tokens, std::string file_name) :
        m_compiler(cmp), m_file_name(file_name), m_tokens(std::move(tokens)) {}

   std::vector<Token> process();

   
   std::string dump();
   std::vector<Token> spit() { return m_tokens; }
private:
   [[nodiscard]] inline std::optional<Token> peek(int offset = 0) const {
      if (m_index + offset >= m_tokens.size()) return {};
      else return m_tokens.at(m_index + offset);
   }

   bool peek_eval(TokenType type, int offset = 0) const { 
      return peek(offset).has_value() && peek(offset).value().type == type; 
   }

   Token consume()   { return m_tokens[m_index++]; }
   bool has_tokens() { return peek().has_value(); }

   bool at_directive() const;
   const Macro* get_macro(const Token& token);

   void handle_directive(std::vector<Token>& out);
   void handle_include  (std::vector<Token>& out, int dir_line);
   void handle_pragma   (int dir_line);
   void handle_define   (int dir_line);
   void expand_into     (const std::vector<Token>& input, std::vector<Token>& out, std::set<std::string>& active, int recursion_depth);
   std::vector<Token> lex_file(const std::string& path, int file_id);

   bool try_expand(const std::vector<Token>& tokens, size_t& position, std::vector<Token>& out, std::set<std::string>& active, int recursion_depth);

   void skip_to_next_line(int dir_line) { while (peek().has_value() && peek().value().line == dir_line) consume(); }

   std::vector<std::vector<Token>> collect_args(const std::vector<Token>& input, size_t& position);
   std::vector<Token> substitute(const Macro& macro, const std::vector<std::vector<Token>>& args);


   Compiler& m_compiler;
   std::string m_file_name;
   std::vector<Token> m_tokens;
   size_t m_index = 0;

   std::set<std::string> m_included;         // #pragma once / guard: alr included paths
   std::vector<std::string> m_include_stack; // for circular detection
   std::unordered_map<std::string, Macro> m_macros;


};

#endif // PREPROCESSOR_H