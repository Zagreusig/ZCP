#ifndef GENERATION_H
#define GENERATION_H

#include "ast/Nodes.h"
#include "lexer/Tokens.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "type_checker.h"
#include <sstream>
#include <map>


class ASMGenerator {
public:
   inline explicit ASMGenerator(NodeProg prog) 
      : m_prog(std::move(prog)), m_types(m_prog) {}

   [[nodiscard]] std::string build();
   void gen_expr(const NodeExpr*);
   void gen_stmt(const NodeStmt*);
   void gen_cond(const NodeCondition*, const std::string&);
   void gen_cond_true(const NodeCondition*, const std::string&);
   void gen_function(const NodeFunction*);
private:
   struct Var {
      std::string name;
      int offset;
      TypeInfo type;
   };

   std::optional<Var> get_var(const std::string&);
   std::string make_label() { return ".L" + std::to_string(m_label_count++); }

   void push(const std::string& reg) {
      m_output << "   push " << reg << "\n";
      m_stack_size++;
   }
   void pop(const std::string& reg) {
      m_output << "   pop " << reg << "\n";
      m_stack_size--;
   }
   void clear(const std::string& reg) { m_output << "   xor " << reg << ", " << reg << "\n"; }

   void emit_consts();
   void emit_print_int();

   void emit_read_chars();
   void emit_read_char();
   void emit_read_int();
   void emit_epilogue();

   void begin_scope();
   void end_scope();

   int count_locals(const NodeScopeBlock*);
   int count_locals(const std::vector<NodeStmt*>&);
   int compute_frame_size(const NodeScopeBlock*);
   int compute_frame_size(const std::vector<NodeStmt*>&);
   int compute_frame_bytes(const std::vector<NodeStmt*>&);
   int have_byte_size(const NodeStmtHave*);

   TypeInfo resolve_have_type(NodeStmtHave*);

   std::string add_string(const std::string&);

   int console_write(const std::string&);   

   bool try_load_simple(const NodeExpr*, const std::string&);

   const NodeProg m_prog;
   std::stringstream _bss;
   std::stringstream _data;
   std::stringstream m_output;
   size_t m_stack_size = 0;
   std::vector<Var> m_vars           {};

   std::vector<std::pair<std::string, std::string>> m_strings {};
   int m_str_count = 0;

   TypeChecker m_types;

   bool m_uses_frame_base = {};
   int m_current_offset   = 0;
   int m_label_count      = 0;
   std::vector<size_t> m_scope_stack {};

};



#endif // GENERATION_H