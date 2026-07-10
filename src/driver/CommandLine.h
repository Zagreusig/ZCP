#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include <optional>
#include <string>
#include <vector>

#include "utils/flags.h"

enum class Flags;

struct FlagSpec {
   char        short_name;  // -a
   const char* long_name;   // --ast
   Flags       flag;        // Flags::PRINT_AST
   bool        takes_value; // false / true
   const char* help;        // HEEEEEEELP
};

struct ParsedArgs {
   std::string input_file;
   std::string output_name = "out";
   std::vector<Flags> flags;
};

namespace CommandLine {
   std::optional<ParsedArgs> parse(int argc, char* argv[]);

   // prints help text, then dies :]
   void print_help(const char* prog_name);
}

#endif // COMMAND_LINE_H