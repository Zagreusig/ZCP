#ifndef ASTPRINTER_H
#define ASTPRINTER_H

#include "Core/Nodes.h"
#include <iostream>
#include <string>

class ASTPrinter {
public:
   explicit ASTPrinter(const NodeProg& prog) : m_prog(prog) {} 

   void print() {
      std::cout << "Program\n";
      for (const NodeFunction* func : m_prog.funcs)
         print_function(func, 1);
   }

   static std::string cmp_name(CmpExprType);
   static std::string bin_name(BinExprType);

private:
   const NodeProg& m_prog;

   void print_function(const NodeFunction*, int);
   void print_stmt(const NodeStmt*, int);
   void print_expr(const NodeExpr*, int);
   void print_condition(const NodeCondition*, int);
   void print_scope(const NodeScopeBlock*, int);

   static std::string pad(int depth) { return std::string(depth * 2, ' '); }
};

#endif // ASTPRINTER_H