#ifndef RUNTIME_ASM_H
#define RUNTIME_ASM_H

#include "utils/msc.h"

enum class Runtime { 
   None       = 0,
   PrintInt   = BIT(0), 
   PrintChar  = BIT(1), 
   PrintBuf   = BIT(2), 
   ReadInt    = BIT(3), 
   ReadChar   = BIT(4), 
   ReadBuf    = BIT(5) 
};
/** TODO: Add floats and bools once developed. */

#endif // RUNTIME_ASM_H