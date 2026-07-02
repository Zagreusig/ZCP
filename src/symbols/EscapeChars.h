#ifndef ESCAPECHARS_H
#define ESCAPECHARS_H

/**
 * ASM Escape sequences:
 * \a (terminal bell):  7 (0x07)
 * \b (backspace):      8 (0x08)
 * \t (tab):            9 (0x09)
 * \n (newline):       10 (0x0A)
 * \v (vert tab):      11 (0x0B)
 * \f (Formfeed / NP): 12 (0x0C)
 * \r (carriage ret):  13 (0x0D)
 * \e (esc):           27 (0x1B)
 */

#include <string>

namespace Esc {
   static char translate_escape(char esc) {
         switch (esc) {
         case 'n':  return '\n';
         case 't':  return '\t';
         case 'r':  return '\r';
         case '\\': return '\\';
         case '"':  return '"';
         case '\'': return '\'';
         case '0':  return '\0';
         default:
            return esc;
      }
   }

   static int asm_code(char esc) {
      switch (esc) {
         case '\n': return 10;
         case '\t': return 9;
         case '\r': return 13;
         case '\0': return 0;
         default:   return 0;
      }
   }

   static bool is_esc_char(char esc) {
      switch (esc) {
         case '\n':
         case '\t':
         case '\r':
         case '\0': return true;
         default:   return false;
      }
   }

   static std::string esc_str(char esc) {
      switch (esc) {
         case '\n': return "'\\n'";
         case '\t': return "'\\t'";
         case '\r': return "'\\r'";
         case '\0': return "'\\0'";
         default:   return "null";
      }
   }
}

#endif // ESCAPECHARS_H