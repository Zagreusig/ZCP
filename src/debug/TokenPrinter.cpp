#include "TokenPrinter.h"

#include <sstream>

#include "Core/Tokens.h"
#include "Core/TokenTable.h"
#include "utils/flags.h"

void TokenPrinter::print(std::ostream& out, const std::vector<Token>& tokens, const FileLookup& lookup) {
   for (const auto& token : tokens) {
      out << "{ " << to_string(token.type);
      if      (token.is_text())  out << ", " << token.text();
      else if (token.is_int())   out << ", " << token.int_val();
      else if (token.is_char())  out << ", " << token.char_val();
      else if (token.is_bool())  out << ", " << token.bool_val();

      out << ", " << lookup(token.fileId) << ":"
                   << token.line << ':' << token.col << " }\n";
   }
   out << std::endl;
}


std::string TokenPrinter::format(std::vector<Token>& tokens, const FileLookup& lookup) {
   std::ostringstream ss;
   print(ss, tokens, lookup);
   return ss.str();
}


std::string TokenPrinter::format_raw(const std::vector<Token>& tokens) {
   std::ostringstream ss;
   for (const auto& token : tokens)
      ss << token.name() << ", ";
   ss << "\n";
   return ss.str();
}


std::string TokenPrinter::format_flags(const std::vector<Flags>& flags) {
   std::ostringstream ss;
   for (const auto& flag : flags) ss << to_str(flag) << " ";
   ss << std::endl;
   return ss.str();
}
