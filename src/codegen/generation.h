#ifndef GENERATION_H
#define GENERATION_H

#include "lexer/lexer.h"
#include "parser/parser.h"
#include <sstream>
#include <map>

class ASMGenerator {
public:
   inline explicit ASMGenerator(NodeProg prog) 
      : m_prog(std::move(prog)) {}

   [[nodiscard]] std::string build();
   void gen_expr(const NodeExpr*);
   void gen_stmt(const NodeStmt*);
   void gen_cond(const NodeCondition*, const std::string&);
   void gen_cond_true(const NodeCondition*, const std::string&);
   void gen_function(const NodeFunction*);
private:
   struct Var {
      std::string name;
      int rbp_offset;
      // Type eventually
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

   void begin_scope();
   void end_scope();

   int count_locals(const NodeScopeBlock*);
   int count_locals(const std::vector<NodeStmt*>&);
   int compute_frame_size(const NodeScopeBlock*);
   int compute_frame_size(const std::vector<NodeStmt*>&);

   const NodeProg m_prog;
   std::stringstream m_output;
   size_t m_stack_size = 0;
   std::vector<Var> m_vars           {};

   int m_current_offset = 0;
   int m_label_count    = 0;
   std::vector<size_t> m_scope_stack {};

};

#endif // GENERATION_H