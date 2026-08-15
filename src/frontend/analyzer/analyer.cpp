#include "analyer.h"

#include <stddef.h>
#include <source_location>
#include <type_traits>
#include <variant>

#include "driver/compiler.h"
#include "Core/TypeConversions.h"
#include "Core/ErrorHandler.h"
#include "utils/msc.h"
#include "Nodes.h"
#include "Layout.h"
#include "SymbolTable.h"
#include "TokenTable.h"
#include "Tokens.h"


void Analyzer::analyze() {
   m_symbols.push_scope(); // global scope
   function_pass();

   for (const NodeTopLevel* tl : m_prog.declarations) {
      if (auto* fn = std::get_if<NodeFunction*>(&tl->variant)) {
         set_curr_func(*m_symbols.lookup((*fn)->name.text()));
         analyze_function(*fn);
      }
      else if (auto* decl = std::get_if<NodeTypeDecl*>(&tl->variant)) {
         analyze_decl(*decl);
      }
      // else if (auto* global = std::get_if<NodeGlobal*>(&tl->variant))
   }
}


void Analyzer::function_pass() {
   for (NodeTopLevel* tl : m_prog.declarations) {
      auto* declPtr = std::get_if<NodeFunction*>(&tl->variant);
      if (!declPtr) continue;
      auto* decl = *declPtr;

      FunctionSymbol func;
      func.name = decl->name.text();
      
      if (decl->has_ret_type)
         func.ret_type.base = Symbols::token_to_datatype(decl->ret_type.type);
      for (const NodeParam& p : decl->params)
         func.params.push_back(p.type);

      func.definition = decl; func.origin_file = decl->name.fileId;

      auto redef = m_symbols.lookup(func.name);
      if (!redef) m_symbols.declare(Symbol(func, decl->name));

      else if (redef->is_function()) {
         if (redef->as_function().definition->body)
            m_compiler.error(decl->name.fileId, decl->name.line, decl->name.col,
                              "Redefinition of function \"" + decl->name.text() + "\".");
         else if (!functions_match(redef->as_function(), func))
            m_compiler.error(decl->name.fileId, decl->name.line, decl->name.col,
                              "Definition of \"" + decl->name.text() + "\" does not match its declaration.");
         else
            m_symbols.replace_in_current(Symbol(func, decl->name)); // stub!
      }
   }
}


void Analyzer::analyze_have(NodeStmtHave* h) {
   TypeInfo info;

   if (!h->decl.type.has_value() && h->expr == nullptr) {
      m_compiler.error
      (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
       "Declaration needs type of initializer.");
      return;
   }

   if (h->decl.type.has_value()) {
      info = h->decl.type.value(); // { base=INT, is_array=false }
      if (h->expr) {
         // is this array lit?
         if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->variant)) {
            const auto& elems = (*lit)->elements;
            for (size_t i = 0; i < elems.size(); i++) {
               TypeInfo et = type_of(elems[i]);
               if (et.base != info.base) {
                  m_compiler.error
                  (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
                   "Array literal has mismatched element types.");
                  break;
               }
            }

            // Annotation gave element type; lit gives shape
            if (info.is_array) {
               // annotation was int[N] - N must be equal lit's len
               if (info.array_len != (int)elems.size())
                  m_compiler.error
                  (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
                   "Array literals doesn't match declared length.");
            } else {
               info.is_array = true;
               info.array_len = (int)elems.size();
            }
         } else {
            TypeInfo init = type_of(h->expr);
            if (!types_match(info, init))
               m_compiler.error
               (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
                "Array type mismatch.");
         }
      }
      // else: annotated, no init
   }
   // ---- no annotation, infer from init -----
   else if (auto* lit = std::get_if<NodeExprArrayLit*>(&h->expr->variant)) {
      const auto& elems = (*lit)->elements;
      if (elems.empty()) {
         m_compiler.error
         (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
          "Cannot infer type of empty array.");
      } else {
         TypeInfo et = type_of(elems[0]);
         for (size_t i = 1; i < elems.size(); i++) {
            if (!types_match(et, type_of(elems[i]))) {
               m_compiler.error
               (h->decl.name.fileId, h->decl.name.line, h->decl.name.col,
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
   declare(h->decl.name, info);
}


void Analyzer::analyze_function(NodeFunction* f) {
   if (!f->body) return; // declaration-only stub.

   mark_in_prog(m_symbols.lookup(f->name.text())->as_function());

   push_scope();
   for (const NodeParam& p : f->params)
      declare(p.name, p.type);

   for (auto* stmt : f->body->stmts)
      analyze_stmt(stmt);
   pop_scope();

   finished_function();
}

/**
 * if:
 *    this condition:
 *       then:
 *          return 99
 *       else:
 *          return 44
 */

TypeInfo Analyzer::find_ret_type(NodeFunction* f) {
   if (!f->body) return TypeInfo{ .base = DataType::NONE };
   if (is_in_prog(f->name.text())) {
      m_compiler.error(f->name.fileId, f->name.line, f->name.col, "Mutual recursion found.");
      return {};
   }
   
   mark_in_prog(m_symbols.lookup(f->name.text())->as_function());
   push_scope();
   for (const NodeParam& param : f->params) declare(param.name, param.type);
   TypeInfo type = find_ret_type(f->body);
   pop_scope();
   finished_function();
   return type;
}


TypeInfo Analyzer::find_ret_type(NodeScopeBlock* block) {
   if (!block) return {};
   TypeInfo type;

   for (auto& stmt : block->stmts) {
      if (auto* _return = std::get_if<NodeStmtReturn*>(&stmt->variant))
         return type_of((*_return)->expr);
      else if (auto* _if = std::get_if<NodeStmtIf*>(&stmt->variant)) { 
         TypeInfo _else = {}; type = find_ret_type((*_if)->body);
         if ((*_if)->else_body) _else = find_ret_type((*_if)->else_body);
         
         if      (type.base != DataType::NONE)  return type;
         else if (_else.base != DataType::NONE) return _else;
      }
      else if (auto* _for = std::get_if<NodeStmtFor*>(&stmt->variant)) {
        type = find_ret_type((*_for)->body);
         if (type.base != DataType::NONE) return type;
      }
      else if (auto* _while = std::get_if<NodeStmtWhile*>(&stmt->variant)) {
         type = find_ret_type((*_while)->body);
         if (type.base != DataType::NONE) return type;
      }
      else if (auto* scope = std::get_if<NodeStmtScope*>(&stmt->variant)) {
         type = find_ret_type((*scope)->scope);
         if (type.base != DataType::NONE) return type;
      }
   }
   return {};
}



void Analyzer::analyze_decl(NodeTypeDecl* t) {
   // throw std::runtime_error("Structs WIP");
   if (auto* _struct = std::get_if<NodeStructDecl*>(&t->variant))
      analyze_struct(*_struct);
   // else if (auto* _enum = std::get_if<NodeEnumDecl*>(&t->variant))
}


void Analyzer::analyze_struct(NodeStructDecl* decl) {
   StructLayout layout;

   for (auto& field : decl->vars) {
      FieldInfo info;
      info.name = field.decl.name.text();
      info.type = field.decl.type.value(); // parser guarantees struct fields have a type
      info.offset = layout.size;
      layout.size += info.type.byte_size();
      layout.fields.push_back(info);
   }

   m_symbols.declare(Symbol{ layout, decl->name });
}


void Analyzer::analyze_scope(NodeScopeBlock* s) {
   push_scope();
   for (auto& stmt : s->stmts) analyze_stmt(stmt);
   pop_scope();
}


void Analyzer::analyze_stmt(NodeStmt* s) {
   std::visit([this](auto* s) {
      using T = std::decay_t<decltype(*s)>;
      if constexpr      (std::is_same_v<T, NodeStmtHave>)  analyze_have(s);
      else if constexpr (std::is_same_v<T, NodeStmtExit>)  type_of(s->expr);
      else if constexpr (std::is_same_v<T, NodeStmtPrint>) type_of(s->expr);
      else if constexpr (std::is_same_v<T, NodeStmtExpr>)  type_of(s->expr);
      else if constexpr (std::is_same_v<T, NodeStmtAssign>) {
         // trgt must exist; RHS -> t should match var's type
         TypeInfo target_t = type_of(s->target);
         TypeInfo rhs_t    = type_of(s->expr);
         if (!types_match(target_t, rhs_t))
            m_compiler.error
            (s->ident.fileId, s->ident.line, s->ident.col,
             "Type mismatch in assignment to '" + s->ident.text() + "'.");
      }
      
      else if constexpr (std::is_same_v<T, NodeStmtReturn>) {
         TypeInfo ret;
         if (s->expr) ret = type_of(s->expr);
         else ret.base = DataType::NONE;

         if (auto* fn = m_symbols.get_function_global(m_curr_func.back().name)) {
            if (fn->definition && !fn->definition->has_ret_type) {
               int line = fn->definition->name.line, col = fn->definition->name.col;
               fn->ret_type = ret; fn->definition->has_ret_type = true;
               fn->definition->ret_type = tok::make(Symbols::datatype_to_token(ret.base), fn->origin_file, line, col);
               m_curr_func.back().ret_type = ret;
            }
         }

         if (m_curr_func.back().ret_type.base != DataType::NONE && !types_match(ret, m_curr_func.back().ret_type))
            m_compiler.error
            (m_curr_func.back().origin_file, m_func_coords.back().first, m_func_coords.back().second,
             "Return type mismatch.");
      }

      else if constexpr (std::is_same_v<T, NodeScopeBlock>) analyze_scope(s);
      

      else if constexpr (std::is_same_v<T, NodeStmtIf>) {
         analyze_condition(s->condition);
         analyze_scope(s->body);

         if (s->else_body) analyze_scope(s->else_body);
      }

      else if constexpr (std::is_same_v<T, NodeStmtWhile>) {
         analyze_condition(s->condition);
         analyze_scope(s->body);
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
   }, s->variant);
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
   }, cond->variant);
}


void Analyzer::push_scope() { m_symbols.push_scope(); }
void Analyzer::pop_scope()  { m_symbols.pop_scope(); }


void Analyzer::push_func() {
   m_curr_func.emplace_back();
   m_func_coords.emplace_back();
}


void Analyzer::push_func(FunctionSymbol func, int line, int col) {
   m_curr_func.push_back(func);
   m_func_coords.push_back({line, col});
}


void Analyzer::pop_func() {
   m_curr_func.pop_back();
   m_func_coords.pop_back();
}


void Analyzer::declare(Token ident, const TypeInfo& t) {
   VariableSymbol var { t };
   Symbol symbol(var, ident);
   if (!m_symbols.declare(symbol))
      m_compiler.error
      (ident.fileId, ident.line, ident.col,
       "Redeclaration of '" + ident.text() + "' in this scope.");
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

      if constexpr      (std::is_same_v<T, NodeExprIntLit>)
         return TypeInfo { .base = DataType::INT };
      
      else if constexpr (std::is_same_v<T, NodeExprCharLit>)
         return TypeInfo { .base = DataType::CHAR };
      
      else if constexpr (std::is_same_v<T, NodeExprBoolLit>)
         return TypeInfo{ .base = DataType::BOOL };

      else if constexpr (std::is_same_v<T, NodeExprStrLit>)
         return TypeInfo { .base = DataType::STR };

      else if constexpr (std::is_same_v<T, NodeExprIdent>) {
         auto found = lookup(node->ident.text());
         if (found.has_value()) return found.value();
         m_compiler.error
         (node->ident.fileId, node->ident.line, node->ident.col,
          "Use of undeclared identifier '" + node->ident.text() + "'.");
         return {};
      }

      else if constexpr (std::is_same_v<T, NodeBinExpr>)
         return type_of(node->left);
      
      else if constexpr (std::is_same_v<T, NodeExprCall>) {
         for (auto* arg : node->args) type_of(arg);
         
         auto symbol = m_symbols.lookup(node->name.text());
         if (!symbol) { m_compiler.error
            (node->name.fileId, node->name.line, node->name.col,
             "Call made to undeclared function '" + node->name.text() + "'.");
            for (auto* arg : node->args) type_of(arg); // still walk args to grab extra problems
            return {};
         }
      
         /** WARN: This will cause issues for bodiless stubs when each TU has separate compilation
          *        instead of the merging of tokens that the preprocessor currently does. */
         if (!symbol->is_function() || !symbol->as_function().definition->body) {
            m_compiler.error
            (node->name.fileId, node->name.line, node->name.col,
             "No matching definition for \"" + symbol->name + "\".");
            return {};
         }

         if (symbol->as_function().ret_type.base == DataType::NONE) {
            NodeFunction* target = symbol->as_function().definition;
            if (is_in_prog(target->name.text())) {
               m_compiler.error
               (node->name.fileId, node->name.line, node->name.col,
                "Mutual recursion found.");
               return {};
            }
            if (target->body && !target->has_ret_type) {
               TypeInfo type = find_ret_type(target);
               if (type.base != DataType::NONE) {
                  if (auto* fn = m_symbols.get_function_global(node->name.text()))
                     fn->ret_type = type;
                  target->has_ret_type = true;
                  target->ret_type = tok::make(Symbols::datatype_to_token(type.base), 
                                               target->name.fileId, target->name.line, target->name.col);
               }
            }
         }

         // arg count check
         if (node->args.size() != symbol->as_function().params.size()) {
            m_compiler.error
            (node->name.fileId, node->name.line, node->name.col,
             std::to_string(symbol->as_function().params.size()) + " argument(s) expected, got " + 
             std::to_string(node->args.size()) + ".");
         }

         // arg type check.
         for (size_t i = 0; i < node->args.size(); i++) {
            TypeInfo at = type_of(node->args[i]);
            if (i < symbol->as_function().params.size() && !types_match(symbol->as_function().params[i], at)) {
               m_compiler.error
               (node->name.fileId, node->name.line, node->name.col,
                "Argument " + std::to_string(i + 1) + " to '" + node->name.text() + 
                "' has mismatched type.");
            }
         }


         return symbol->as_function().ret_type;
      }

      else if constexpr (std::is_same_v<T, NodeExprIncDec>) return lookup(node->ident.text()).value_or(TypeInfo{});
      else if constexpr (std::is_same_v<T, NodeExprRead>) {
         switch (node->kind) {
            case DataType::CHAR: return TypeInfo { .base = DataType::CHAR };
            case DataType::INT:  return TypeInfo { .base = DataType::INT };
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
         auto arr = lookup(node->ident.text());
         if (!arr.has_value()) { 
            m_compiler.error
            (node->ident.fileId, node->ident.line, node->ident.col,
             "Undeclared array: " + node->ident.text()); return {}; }
         if (!arr.value().is_array && arr.value().base != DataType::STR) 
            { m_compiler.error
               (node->ident.fileId, node->ident.line, node->ident.col,
                "Cannot index non-array."); return {}; }
         // indexing array yields its ELEMENT type (scalar, not array)
         return TypeInfo { .base = (arr.value().base == DataType::STR ? DataType::CHAR : arr.value().base) }; // is_array = false (default)
      }
   
      else if constexpr (std::is_same_v<T, NodeExprUnary>) {
         TypeInfo operand_t = type_of(node->operand);
         if (node->op == UnaryExprType::NOT) return TypeInfo { .base = DataType::BOOL };
         return operand_t; // NEGATE: same type as operand
      }

      else if constexpr (std::is_same_v<T, NodeExprField>) {
         TypeInfo member;
         if (auto info = lookup(node->field.text()); info.has_value()) member = info.value();
         else { m_compiler.error(node->field.fileId, node->field.line, node->field.col, "Unknown field '" + node->field.text() + "'."); return {}; }
         m_compiler.error(-1, 0, 0, "MEMBER FIELD WIP");
         return {};
      }

      else if constexpr (std::is_same_v<T, NodeExprNew>) { m_compiler.error(-1, 0, 0, "NEW EXPR WIP"); return {}; }
      else variant_get<T>(node, std::source_location::current());
   }, expr->variant);
}


std::optional<TypeInfo> Analyzer::lookup(const std::string& ident) {
   const Symbol* sym = m_symbols.lookup(ident);
   if (sym && sym->is_variable()) return sym->as_variable().type;
   return {};
}


bool Analyzer::types_match(TypeInfo t1, TypeInfo t2) {
   return t1.base == t2.base && t1.is_array == t2.is_array &&
          t1.array_len == t2.array_len && t1.is_ptr == t2.is_ptr &&
          t1.is_signed == t2.is_signed && 
          t1.struct_layout == t2.struct_layout;
}


bool Analyzer::functions_match(FunctionSymbol first, FunctionSymbol second) {
   bool ret = true;
   if (first.params.size() != second.params.size()) return false;
   for (size_t i = 0; i < first.params.size(); i++) {
      ret = ret && types_match(first.params.at(i), second.params.at(i));
   }
   return ret && (first.name == second.name) &&
          types_match(first.ret_type, second.ret_type);
}


void Analyzer::set_curr_func(Symbol symbol) {
   if (!symbol.is_function()) return;
   push_func(symbol.as_function(), symbol.decl.line, symbol.decl.col);
}


void Analyzer::replace_curr_func(Symbol symbol) {
   if (!symbol.is_function()) return;
   pop_func();
   set_curr_func(symbol);
}


bool Analyzer::is_analyzed(const std::string& name) {
   for (auto it = m_analyzed.rbegin(); it != m_analyzed.rend(); ++it)
      if (it->name == name) return true;
   return false;
}


bool Analyzer::is_in_prog(const std::string& name) {
   for (auto it = m_in_prog.rbegin(); it != m_in_prog.rend(); ++it)
      if (it->name == name) return true;
   return false;
}


void Analyzer::mark_in_prog(const FunctionSymbol& function) {
   if (is_in_prog(function.name)) return;
   m_in_prog.push_back(function);
}


void Analyzer::finished_function() {
   m_analyzed.push_back(m_in_prog.back());
   m_in_prog.pop_back();
}


void Analyzer::resolve_struct_type(TypeInfo& info, Token at) {
   if (info.base != DataType::STRUCT || info.struct_layout) return; // already resolved
   
   const Symbol* sym = m_symbols.lookup(info.unresolved_name);
   if (!sym || !sym->is_type())
      m_compiler.error(at.fileId, at.line, at.col, "Unknown type '" + info.unresolved_name + "'.");
   else
      info.struct_layout = &sym->as_type().layout;
}