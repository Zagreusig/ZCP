#ifndef ASTPRINTER_H
#define ASTPRINTER_H

#include <iostream>
#include <string>
#include <vector>

#include "Core/Nodes.h"

enum class BinExprType;
enum class ComparisonOp;
enum class LogicOp;

class ASTPrinter {
public:
   explicit ASTPrinter(const NodeProg& prog, std::ostream& out) 
      : m_prog(prog), m_out(out) {} 

   void print() {
      m_out << "Program\n";
      for (const NodeTopLevel* top : m_prog.declarations)
         print_top(top, 1);
      // for (const NodeFunction* func : m_prog.functions)
      //    print_function(func, 1);
   }

   static std::string cmp_name(ComparisonOp);
   static std::string logic_name(LogicOp);
   static std::string bin_name(BinExprType);

private:
   const NodeProg& m_prog;
   std::ostream& m_out;

   void print_top(const NodeTopLevel*, int);
   void print_type_decl(const NodeTypeDecl*, int);
   void print_struct_decl(const NodeStructDecl*, int);
   void print_function(const NodeFunction*, int);
   void print_stmt(const NodeStmt*, int);
   void print_expr(const NodeExpr*, int);
   void print_condition(const NodeCondition*, int);
   void print_scope(const NodeScopeBlock*, int);

   static std::string pad(int depth) { return std::string(depth * 2, ' '); }
};

#endif // ASTPRINTER_H