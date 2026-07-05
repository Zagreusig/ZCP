#ifndef FLAGS_H
#define FLAGS_H

#include "msc.h"
#include <stdint.h>
#include <string>
#include <vector>
   
// ===========================================================================
// Program flags!
// ===========================================================================

// Flags enum are bitmasked to be able to combine multiple flags
// into a single variable.
enum class Flags {
   NONE            = 0,
   USER_NAME       = BIT(0),   // -o  
   PRINT_TOKENS    = BIT(1),   // -t  
   PRINT_FLAGS     = BIT(2),   // -f
   PRINT_AST       = BIT(3),   // -s
   LEAVE_ASM       = BIT(4),   // -a
   LEAVE_OBJ       = BIT(5),   // -j
   HALLLLLPUH      = BIT(6),   // -h
   PRESERVE_PRE_OP = BIT(7)    // -p
};

inline Flags operator|(Flags a, Flags b) {
   return static_cast<Flags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Flags combine_flags(std::vector<Flags> flags) {
   Flags t = {};
   for (auto& flag : flags) t = flag | t;
   return t;
}

inline std::string to_str(Flags flag) {
   switch (flag) {
      case Flags::NONE:            return "NONE";
      case Flags::USER_NAME:       return "USER_NAME";
      case Flags::PRINT_TOKENS:    return "PRINT_TOKENS";
      case Flags::PRINT_FLAGS:     return "PRINT_FLAGS";
      case Flags::PRINT_AST:       return "PRINT_AST";
      case Flags::LEAVE_ASM:       return "LEAVE_ASM";
      case Flags::LEAVE_OBJ:       return "LEAVE_OBJ";
      case Flags::PRESERVE_PRE_OP: return "PRESERVE_PRE_OP";
      default:                     return "ERR_FLAG";
   }
}

#endif // FLAGS_H