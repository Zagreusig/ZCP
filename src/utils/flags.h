#ifndef FLAGS_H
#define FLAGS_H

#define BIT(x) (1 << x)   

#include <string>

enum class Flags {
   NONE         = 0,
   USER_NAME    = BIT(1),   // -o  
   PRINT_TOKENS = BIT(2),   // -t  
   PRINT_FLAGS  = BIT(3),   // -f
   PRINT_AST    = BIT(4),   // -s
   LEAVE_ASM    = BIT(5),   // -a
   LEAVE_OBJ    = BIT(6),   // -j
};

inline Flags operator|(Flags a, Flags b) {
   return static_cast<Flags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

static std::string to_str(Flags flag) {
   switch (flag) {
      case Flags::NONE:         return "NONE";
      case Flags::USER_NAME:    return "USER_NAME";
      case Flags::PRINT_TOKENS: return "PRINT_TOKENS";
      case Flags::PRINT_FLAGS:  return "PRINT_FLAGS";
      case Flags::PRINT_AST:    return "PRINT_AST";
      case Flags::LEAVE_ASM:    return "LEAVE_ASM";
      case Flags::LEAVE_OBJ:    return "LEAVE_OBJ";
      default:                  return "ERR_FLAG";
   }
}

#endif // FLAGS_H