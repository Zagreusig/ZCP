#ifndef ANALYER_H
#define ANALYER_H

#include "ast/Nodes.h"
#include "ErrAndRep/ErrorHandler.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct FuncSig {
   std::vector<TypeInfo> param_types;
   TypeInfo ret_type;
   Token ident;
};

class Analyzer {
public:
   Analyzer(NodeProg& prog, Diagnostics& diag)
      : m_prog(prog), m_diag(diag) {}
   void analyze(); // walks AST for any issues.

private:
   void analyze_condition(const NodeCondition*);
   void analyze_function(NodeFunction*);
   void analyze_stmt(NodeStmt*);
   void analyze_have(NodeStmtHave*);
   TypeInfo type_of(NodeExpr*);
   TypeInfo compute_type_of(const NodeExpr*); // resolving expression types

   void push_scope();
   void pop_scope();
   void declare(const std::string&, const TypeInfo&);
   std::optional<TypeInfo> lookup(const std::string&);

   bool types_match(TypeInfo, TypeInfo);

   NodeProg& m_prog;
   Diagnostics& m_diag;
   std::unordered_map<std::string, FuncSig> m_func_sigs;
   std::vector<std::pair<std::string, TypeInfo>> m_vars;
   std::vector<size_t> m_scopes;

   FuncSig m_curr_func;
};

#endif // ANALYER_H