#ifndef FORMAT_H
#define FORMAT_H

#include "Core/Tokens.h"
#include "Core/TokenTable.h"
#include "utils/flags.h"

#include <functional>
#include <vector>
#include <string>
#include <sstream>
#include <ostream>

namespace Format {
   using FileLookup = std::function<std::string(int)>;
   inline std::string print(const std::vector<Token>& tokens, const FileLookup& lookup) {
      std::ostringstream os;
      for (const auto& token : tokens) {
         os << "{" << to_string(token.type);
         if (token.has_value()) os << ", " << token.value_str();
         os << ", " << lookup(token.fileId) << ":" << token.line << ':' << token.col << "}\n";
      }

      return os.str();
   }

   
   inline std::string print(const std::vector<Token>& tokens) {
      std::ostringstream os;
      for (const auto& token : tokens) {
         os << "{" << to_string(token.type);
         if (token.has_value()) os << ", " << token.value_str();
         os << "}\n";
      }

      return os.str();
   }


   inline std::string print(const std::vector<Flags>& flags) {
      std::ostringstream os;
      for (const auto& flag : flags) os << to_str(flag) << " ";
      os << std::endl;
      return os.str();
   }
}

#endif // FORMAT_H