#ifndef ANALYER_H
#define ANALYER_H

#include <optional>
#include <string>
#include <vector>
#include <utility>

#include "Core/Nodes.h"
#include "Core/ErrorHandler.h"
#include "Core/SymbolTable.h"
#include "Tokens.h"

class Compiler;
struct Token;

class Analyzer {
public:
   Analyzer(Compiler& cmp, NodeProg& prog)
      : m_prog(prog), m_compiler(cmp) {}
   void analyze(); // walks AST for any issues.

private:
   void function_pass();

   void analyze_condition(const NodeCondition*);
   void analyze_function(NodeFunction*);
   void analyze_decl(NodeTypeDecl*);
   void analyze_struct(NodeStructDecl*);
   void analyze_stmt(NodeStmt*);
   void analyze_have(NodeStmtHave*);
   void analyze_scope(NodeScopeBlock*);
   TypeInfo type_of(NodeExpr*);
   TypeInfo compute_type_of(const NodeExpr*); // resolving expression types

   TypeInfo find_ret_type(NodeFunction*);
   TypeInfo find_ret_type(NodeStmt*);
   TypeInfo find_ret_type(NodeScopeBlock*);

   void push_scope();
   void pop_scope();
   void push_func();
   void push_func(FunctionSymbol func, int line, int col);
   void pop_func();
   void declare(Token, const TypeInfo&);
   std::optional<TypeInfo> lookup(const std::string&);

   bool types_match(TypeInfo, TypeInfo);
   bool functions_match(FunctionSymbol, FunctionSymbol);

   void set_curr_func(Symbol symbol);
   void replace_curr_func(Symbol symbol);

   bool is_analyzed(const std::string& name);
   bool is_in_prog(const std::string& name);
   void mark_in_prog(const FunctionSymbol& function);
   void finished_function();

   NodeProg& m_prog;
   Compiler& m_compiler;
   SymbolTable m_symbols;

   std::vector<FunctionSymbol> m_analyzed = {};
   std::vector<FunctionSymbol> m_in_prog = {};
   std::vector<FunctionSymbol> m_curr_func = {};
   std::vector<std::pair<int, int>> m_func_coords = {};
};

#endif // ANALYER_H