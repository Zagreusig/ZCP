#ifndef ANALYER_H
#define ANALYER_H

#include <optional>
#include <string>

#include "Core/Nodes.h"
#include "Core/ErrorHandler.h"
#include "Core/SymbolTable.h"
#include "Tokens.h"

class Compiler;

class Analyzer {
public:
   Analyzer(Compiler& cmp, NodeProg& prog)
      : m_prog(prog), m_compiler(cmp) {}
   void analyze(); // walks AST for any issues.

private:
   void analyze_condition(const NodeCondition*);
   void analyze_function(NodeFunction*);
   void analyze_decl(NodeTypeDecl*);
   void analyze_struct(NodeStructDecl*);
   void analyze_stmt(NodeStmt*);
   void analyze_have(NodeStmtHave*);
   TypeInfo type_of(NodeExpr*);
   TypeInfo compute_type_of(const NodeExpr*); // resolving expression types

   void push_scope();
   void pop_scope();
   void declare(Token, const TypeInfo&);
   std::optional<TypeInfo> lookup(const std::string&);

   bool types_match(TypeInfo, TypeInfo);
   bool functions_match(FunctionSymbol, FunctionSymbol);

   void set_curr_func(Symbol symbol);

   NodeProg& m_prog;
   Compiler& m_compiler;
   SymbolTable m_symbols;

   FunctionSymbol m_curr_func = {};
   int m_func_line = 0, m_func_col = 0;
};

#endif // ANALYER_H