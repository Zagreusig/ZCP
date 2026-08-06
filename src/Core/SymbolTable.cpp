#include "SymbolTable.h"

#include <assert.h>
#include <utility>

void SymbolTable::push_scope() {
   m_scopes.emplace_back();
}


void SymbolTable::pop_scope() {
   assert(!m_scopes.empty());
   m_scopes.pop_back();
}


bool SymbolTable::declare(const Symbol& symbol) {
   assert(!m_scopes.empty());
   if (auto declared = lookup_in_current(symbol.name)) return false;
   m_scopes.back()[symbol.name] = symbol;
   return true;
}


void SymbolTable::replace_in_current(const Symbol& symbol) {
   assert(!m_scopes.empty());
   m_scopes.back()[symbol.name] = symbol;
}


void SymbolTable::replace_in_global(const Symbol& symbol) {
   assert(!m_scopes.empty());
   m_scopes.front()[symbol.name] = symbol;
}


const Symbol* SymbolTable::lookup(const std::string& name) const {
   for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it)
      if (it->count(name) > 0) return &it->find(name)->second;
   return nullptr; // undeclared
}


const Symbol* SymbolTable::lookup_in_current(const std::string& name) const {
   return m_scopes.back().count(name) > 0 ? &m_scopes.back().at(name) : nullptr;
}


/**
* Gets a mutable pointer so that ret_type can be inferred correctly. 
*/
FunctionSymbol* SymbolTable::get_function_global(const std::string& name) {
   assert(!m_scopes.empty());
   auto iterator = m_scopes.front().find(name);
   if (iterator == m_scopes.front().end() || !iterator->second.is_function()) return nullptr;
   auto* test = std::get_if<FunctionSymbol>(&iterator->second.info);
   if (test) return test;
   else return nullptr;
}