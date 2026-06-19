#include "type_checker.h"
#include "ast/Nodes.h"
#include "lexer/Tokens.h"

DataType map_token_type(const TokenType t) {
   switch (t) {
      case TokenType::INT:
      case TokenType::INT_LIT:  return DataType::INT;  break;
      case TokenType::CHAR:
      case TokenType::CHAR_LIT: return DataType::CHAR; break;
      case TokenType::STR:
      case TokenType::STR_LIT:  return DataType::STR;  break;
      default:                  return DataType::NONE;
   }
}

TypeChecker::TypeChecker(const NodeProg& prog) {
   for (const NodeFunction* f : prog.funcs) {
      DataType ret = f->has_ret_type ? map_token_type(f->ret_type.type)
                                     : DataType::NONE;
      m_func_ret[f->name.value.value()] = ret;
   }
}


DataType TypeChecker::type_of(const NodeExpr* expr) const {
   if (!expr) return DataType::NONE;

   return std::visit([this](auto* node) -> DataType {
      using T = std::decay_t<decltype(*node)>;
      if constexpr (std::is_same_v<T, NodeExprIntLit>)
         return DataType::INT;
      else if constexpr (std::is_same_v<T, NodeExprCharLit>)
         return DataType::CHAR;
      else if constexpr (std::is_same_v<T, NodeExprStrLit>)
         return DataType::STR;
      else if constexpr (std::is_same_v<T, NodeExprIdent>)
         return var_type(node->ident.value.value());
      else if constexpr (std::is_same_v<T, NodeBinExpr>)
         return type_of(node->left);
      else if constexpr (std::is_same_v<T, NodeExprIncDec>)
         return var_type(node->ident.value.value());
      else if constexpr (std::is_same_v<T, NodeExprCall>) {
         auto it = m_func_ret.find(node->name.value.value());
         return it != m_func_ret.end() ? it->second : DataType::NONE;
      }
      else return DataType::NONE;
   }, expr->var);
}


void TypeChecker::declare_var(const std::string& str, DataType type) {
   m_vars.push_back({str, type});
}


void TypeChecker::push_scope() {
   m_scopes.push_back(m_vars.size());
}


void TypeChecker::pop_scope() {
   size_t var_before = m_scopes.back();
   m_scopes.pop_back();
   m_vars.resize(var_before);
}

std::string TypeChecker::name_of(DataType t) {
   switch (t) {
      case DataType::INT:  return "INT";
      case DataType::CHAR: return "CHAR";
      case DataType::STR:  return "STR";
      case DataType::BOOL: return "BOOL";
      default:             return "NONE";
   }
}


DataType TypeChecker::var_type(const std::string& str) const {
   for (auto it = m_vars.rbegin(); it != m_vars.rend(); ++it)
      if (it->first == str) return it->second;
   return DataType::NONE;
}