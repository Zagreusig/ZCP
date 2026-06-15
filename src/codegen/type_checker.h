#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast/Nodes.h"
#include <string>
#include <unordered_map>
#include <vector>

enum class DataType { NONE, INT, CHAR, STR, BOOL };

DataType map_token_type(const TokenType);

class TypeChecker {
public:
   explicit TypeChecker(const NodeProg& prog);

   DataType type_of(const NodeExpr*) const;

   // Scope tracking
   void declare_var(const std::string& name, DataType);
   void push_scope();
   void pop_scope();

   static std::string name_of(DataType); // For printing / errors

private:
   DataType var_type(const std::string&) const;

   std::unordered_map<std::string, DataType> m_func_ret;
   std::vector<std::pair<std::string, DataType>> m_vars;
   std::vector<size_t> m_scopes;
};

#endif // TYPE_CHECKER_H