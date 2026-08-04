#ifndef TOKENPRINTER_H
#define TOKENPRINTER_H

#include <functional>
#include <ostream>
#include <string>
#include <vector>

struct Token;
enum class Flags;

namespace TokenPrinter {
   // Resolves a Token::fileId to a display name; decouples this from
   // knowing about Compiler::m_files directly.
   using FileLookup = std::function<std::string(int)>;

   void print(std::ostream& out, const std::vector<Token>& tokens, const FileLookup& lookup);
   std::string format(std::vector<Token>& tokens, const FileLookup& lookup);
   std::string format_raw(const std::vector<Token>& tokens);
   std::string format_flags(const std::vector<Flags>& flags);
}

#endif // TOKENPRINTER_H
