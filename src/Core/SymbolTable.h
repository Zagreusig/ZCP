#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <string>
#include <source_location>
#include <unordered_map>
#include <variant>
#include <vector>
#include "ErrorHandler.h"
#include "Tokens.h"
#include "Nodes.h"
#include "Layout.h"

struct VariableSymbol { TypeInfo type; };
struct FunctionSymbol {       // absorbing analyzer's FunctionSig table
   std::string name;
   TypeInfo ret_type;
   std::vector<TypeInfo> params;
   int origin_file = 0;
   NodeFunction* definition;
};
struct TypeSymbol { // when more things exist, std::variant<StructLayout, EnumLayout>
   StructLayout layout;
};

struct Symbol {
   std::string name;
   Token decl;         // where it's declared (diagnostics)
   std::variant<VariableSymbol, FunctionSymbol, TypeSymbol> info;

   Symbol() {}
   Symbol(StructLayout& layout, Token tok) : 
      name(tok.text()), decl(tok), info(TypeSymbol { layout }) {}
   Symbol(VariableSymbol& var, Token tok) : 
      name(tok.text()), decl(tok), info(var) {}
   Symbol(FunctionSymbol& fn, Token tok) : 
      name(tok.text()), decl(tok), info(fn) {}

   // For convenience :D
   bool is_variable() const { return std::holds_alternative<VariableSymbol>(info); }
   bool is_function() const { return std::holds_alternative<FunctionSymbol>(info); }
   bool is_type()     const { return std::holds_alternative<TypeSymbol>(info); }

   // const FunctionSymbol& as_function() const { return std::get<FunctionSymbol>(info); }
   // const VariableSymbol& as_variable() const { return std::get<VariableSymbol>(info); }
   // const TypeSymbol&     as_type()     const { return std::get<TypeSymbol>(info); }
   const FunctionSymbol& as_function(std::source_location loc = std::source_location::current()) const { return variant_get<FunctionSymbol>(info, loc); }
   const VariableSymbol& as_variable(std::source_location loc = std::source_location::current()) const { return variant_get<VariableSymbol>(info, loc); }
   const TypeSymbol&     as_type(std::source_location loc = std::source_location::current())     const { return variant_get<TypeSymbol>(info, loc); }
};


class SymbolTable {
   std::vector<std::unordered_map<std::string, Symbol>> m_scopes;
public:
   SymbolTable() { m_scopes.reserve(50); }
   
   void push_scope();
   void pop_scope();
   bool declare(const Symbol& symbol);    // false if redeclared in current scope
   void replace_in_current(const Symbol& symbol);
   void replace_in_global(const Symbol& symbol); 
   const Symbol* lookup(const std::string& name) const;  // nearest enclosing, or null
   const Symbol* lookup_in_current(const std::string& name) const; // for redcl checks
   FunctionSymbol* get_function_global(const std::string& name);
};

#endif // SYMBOLTABLE_H