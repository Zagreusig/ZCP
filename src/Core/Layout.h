#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>
#include <vector>
#include "Nodes.h"

struct FieldInfo { std::string name; TypeInfo type; int offset; };
struct StructLayout {
   std::vector<FieldInfo> fields = {};
   int size = 0, alignment = 16;

   const FieldInfo* field(const std::string& name) const {
      for (auto& var : fields)
         if (var.name == name) return &var;
      return nullptr;
   }
};

#endif // LAYOUT_H