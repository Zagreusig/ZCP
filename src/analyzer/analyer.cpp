#include "analyer.h"
#include "driver/compiler.h"
#include "symbols/SymbolTable.h"
#include "utils/msc.h"
#include <iostream>
#include <string.h>

void Analyzer::analyze() {
   for (const NodeFunction* f : m_prog.funcs) {
      FuncSig sig;
      sig.ident = f->name;
      if (f->has_ret_type)
         sig.ret_type.base = Symbols::tok_dt(f->ret_type.type);
   
      for (const NodeParam& p : f->params)
         sig.param_types.push_back(p.type);
      m_func_sigs[f->name.value.value()] = sig;
   }
   
   for (NodeFunction* f : m_prog.funcs) {
      m_curr_func = m_func_sigs[f->name.value.value()];
      analyze_function(f);
   }
}


void Analyzer::analyze_have(NodeStmtHave* h) {
   TypeInfo info;

   if (!h->has_type && h->expr == nullptr) {
      m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col,
                   "Declaration needs type of initializer.");
      return;
   }

   if (h->has_type) {
      info = h->decl_type; // { base=INT, is_array=false }
      if (h->expr) {
         // is this array lit?
         if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->var)) {
            const auto& elems = (*lit)->elements;
            for (size_t i = 0; i < elems.size(); i++) {
               TypeInfo et = type_of(elems[i]);
               if (et.base != info.base) {
                  m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col,
                              "Array literal has mismatched element types.");
                  break;   
               } 
            }
      
            // Annotation gave element type; lit gives shape
            if (info.is_array) {
               // annotation was int[N] - N must be equal lit's len
               if (info.array_len != (int)elems.size())
                  m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col,
                              "Array literals doesn't match declared length.");
            } else {
               info.is_array = true;
               info.array_len = (int)elems.size();
            }
         } else {
            TypeInfo init = type_of(h->expr);
            if (!types_match(info, init))
               m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col, 
                           "Array type mismatch.");
         }
      }
      // else: annotated, no init
   }
   // ---- no annotation, infer from init -----
   else if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->var)) {
      const auto& elems = (*lit)->elements;
      if (elems.empty()) {
         m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col,
                      "Cannot infer type of empty array.");
      } else {
         TypeInfo et = type_of(elems[0]);
         for (size_t i = 1; i < elems.size(); i++) {
            if (!types_match(et, type_of(elems[i]))) {
               m_ctx.diag.error(CompPhase::Analysis, h->ident.line, h->ident.col,
                            "Array literal has mismatched element types.");
               break;
            }
         }
         info.base = et.base;
         info.is_array = true;
         info.array_len = (int)elems.size();
      }
   }
   else {
      info = type_of(h->expr); // have x = 5;
   }

   h->resolved = info;
   h->is_resolved = true;
   declare(h->ident.value.value(), info);
}


void Analyzer::analyze_function(NodeFunction* f) {
   push_scope();
   for (const NodeParam& p : f->params)
      declare(p.name.value.value(), p.type);

   for (auto* stmt : f->body->stmts)
      analyze_stmt(stmt);
   pop_scope();
}


void Analyzer::analyze_stmt(NodeStmt* s) {
   std::visit([this](auto* s) {
      using T = std::decay_t<decltype(*s)>;
      if constexpr (std::is_same_v<T, NodeStmtHave>)
         analyze_have(s);
      
      else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
         // trgt must exist; RHS -> t should match var's type
         TypeInfo target_t = type_of(s->target);
         TypeInfo rhs_t    = type_of(s->expr);
         if (!types_match(target_t, rhs_t))
            m_ctx.diag.error(CompPhase::Analysis, s->ident.line, s->ident.col,
                            "Type mismatch in assignment to '" + s->ident.value.value() + "'.");
      }
      
      else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
         TypeInfo ret = type_of(s->expr);
         if (m_curr_func.ret_type.base != DataType::NONE && !types_match(ret, m_curr_func.ret_type))
            m_ctx.diag.error(CompPhase::Analysis, m_curr_func.ident.line, m_curr_func.ident.col,
                         "Return type mismatch.");
      }
       
      else if constexpr (std::is_same_v<T, NodeStmtExit>)
         type_of(s->expr);
      
      else if constexpr (std::is_same_v<T, NodeStmtPrint>)
         type_of(s->expr);
      
      else if constexpr (std::is_same_v<T, NodeStmtExpr>)
         type_of(s->expr);
      
      else if constexpr (std::is_same_v<T, NodeScopeBlock>) {
         push_scope();
         for (auto* inner : s->stmts) analyze_stmt(inner);
         pop_scope();
      }
      else if constexpr (std::is_same_v<T, NodeStmtIf>) {
         analyze_condition(s->condition);
         push_scope();
         for (auto* inner : s->body->stmts) analyze_stmt(inner);
         pop_scope();

         if (s->else_body) {
            push_scope();
            for (auto* inner : s->else_body->stmts) analyze_stmt(inner);
            pop_scope();
         }
      }

      else if constexpr (std::is_same_v<T, NodeStmtWhile>) {
         analyze_condition(s->condition);
         push_scope();
         for (auto* inner : s->body->stmts) analyze_stmt(inner);
         pop_scope();
      }

      else if constexpr (std::is_same_v<T, NodeStmtFor>) {
         push_scope();
         analyze_stmt(s->init);
         analyze_condition(s->condition);
         analyze_stmt(s->increment);
         for (auto* inner : s->body->stmts) analyze_stmt(inner);
         pop_scope();
      }
      else return;
   }, s->var);
}


void Analyzer::analyze_condition(const NodeCondition* cond) {
   std::visit([this](auto* c) {
      using T = std::decay_t<decltype(*c)>;
      if constexpr (std::is_same_v<T, NodeCmpCondition>) {
         type_of(c->left);
         if (c->right) type_of(c->right);
      }
      else if constexpr (std::is_same_v<T, NodeLogicCondition>) {
         analyze_condition(c->left);
         analyze_condition(c->right);
      }
      else return;
   }, cond->var);
}


void Analyzer::push_scope() {
   m_scopes.push_back(m_vars.size());
}


void Analyzer::pop_scope() {
   size_t before = m_scopes.back();
   m_scopes.pop_back();
   m_vars.resize(before);
}


void Analyzer::declare(const std::string& ident, const TypeInfo& t) {
   m_vars.push_back({ ident, t });
}


TypeInfo Analyzer::type_of(NodeExpr* expr) {
   if (!expr) return {};
   TypeInfo t = compute_type_of(expr);
   expr->resolved = t;
   expr->is_resolved = true;
   return t;
}


TypeInfo Analyzer::compute_type_of(const NodeExpr* expr) {
   if (!expr) return {};

   return std::visit([this](auto* node) -> TypeInfo {
      using T = std::decay_t<decltype(*node)>;

      if constexpr (std::is_same_v<T, NodeExprIntLit>)
         return TypeInfo { .base = DataType::INT };
      
      else if constexpr (std::is_same_v<T, NodeExprCharLit>)
         return TypeInfo { .base = DataType::CHAR };
      
      else if constexpr (std::is_same_v<T, NodeExprStrLit>)
         return TypeInfo { .base = DataType::STR };

      else if constexpr (std::is_same_v<T, NodeExprIdent>) {
         auto found = lookup(node->ident.value.value());
         if (found.has_value()) return found.value();
         m_ctx.diag.error(CompPhase::Analysis, node->ident.line, node->ident.col,
                      "Use of undeclared identifier '" + node->ident.value.value() + "'.");
         return {};
      }

      else if constexpr (std::is_same_v<T, NodeBinExpr>)
         return type_of(node->left);
      
      else if constexpr (std::is_same_v<T, NodeExprCall>) {
         for (auto* arg : node->args)
            type_of(arg); // stamping the arg as resolved; no return needed here
         auto it = m_func_sigs.find(node->name.value.value());
         
         if (it == m_func_sigs.end()) {
            m_ctx.diag.error(CompPhase::Analysis, node->name.line, node->name.col,
                         "Call made to undeclared function '" + node->name.value.value() + "'.");
         
            for (auto* arg : node->args) type_of(arg); // still walk args to grab extra problems
            return {};
         }
      
         const FuncSig& sig = it->second;

         // arg count check
         if (node->args.size() != sig.param_types.size()) {
            m_ctx.diag.error(CompPhase::Analysis, node->name.line, node->name.col,
                         std::to_string(sig.param_types.size()) + " argument(s) expected, got " + 
                         std::to_string(node->args.size()) + ".");
         }

         // arg type check.
         for (size_t i = 0; i < node->args.size(); i++) {
            TypeInfo at = type_of(node->args[i]);
            if (i < sig.param_types.size() && !types_match(sig.param_types[i], at)) {
               m_ctx.diag.error(CompPhase::Analysis, node->name.line, node->name.col,
                            "Argument " + std::to_string(i + 1) + " to '" + node->name.value.value() + 
                            "' has mismatched type.");
            }
         }

         return sig.ret_type;
      }

      else if constexpr (std::is_same_v<T, NodeExprIncDec>)
         return lookup(node->ident.value.value()).value_or(TypeInfo{});
      
      else if constexpr (std::is_same_v<T, NodeExprRead>) {
         switch (node->kind) {
            case ReadKind::Char: return TypeInfo { .base = DataType::CHAR };
            case ReadKind::Int:  return TypeInfo { .base = DataType::INT };
            default:             return TypeInfo { .base = DataType::INT };
         }
      }

      else if constexpr (std::is_same_v<T, NodeExprArrayLit>) {
         if (node->elements.empty()) return {};
         TypeInfo et = type_of(node->elements[0]);
         return TypeInfo { .base = et.base, .is_array = true,
                           .array_len = (int)node->elements.size() };
      }

      else if constexpr (std::is_same_v<T, NodeExprIndex>) {
         auto arr = lookup(node->ident.value.value());
         if (!arr.has_value()) { 
            m_ctx.diag.error(CompPhase::Analysis, node->ident.line, node->ident.col,
                         "This broken...."); return {}; }
         if (!arr.value().is_array && arr.value().base != DataType::STR) 
            { m_ctx.diag.error(CompPhase::Analysis, node->ident.line, node->ident.col,
                           "Cannot index non-array."); return {}; }
         // indexing array yields its ELEMENT type (scalar, not array)
         return TypeInfo { .base = (arr.value().base == DataType::STR ? DataType::CHAR : arr.value().base) }; // is_array = false (default)
      }
   
      else static_assert(always_false<T>, "Unhandled node.");
   }, expr->var);
}


std::optional<TypeInfo> Analyzer::lookup(const std::string& ident) {
   for (auto it = m_vars.rbegin(); it != m_vars.rend(); ++it)
      if (it->first == ident) return it->second;
   return {};
}


bool Analyzer::types_match(TypeInfo t1, TypeInfo t2) {
   if (t1.base != t2.base) return false;
   if (t1.is_array != t2.is_array) return false;
   if (t1.is_array && t1.array_len != t2.array_len) return false;
   return true;
}
