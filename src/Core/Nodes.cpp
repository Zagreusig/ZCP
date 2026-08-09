#include "Nodes.h"
#include "Layout.h"

/**
 * moved here to avoid circular include
 */

int TypeInfo::element_size() const {
   switch (base) {
      case DataType::BOOL:
      case DataType::CHAR:   return 1;
      case DataType::INT:    return 8;
      case DataType::STRUCT: return struct_layout->size;
      default:               return 8;
   }
}

int TypeInfo::byte_size() const {
   if (base == DataType::STR) return 16; // fat pointer: ptr(8) + len(8)
   if (base == DataType::STRUCT && struct_layout) return struct_layout->size;
   return is_array ? array_len * element_size() : element_size();
}