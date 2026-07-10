#include "CommandLine.h"

#include <cstdio>
#include <iostream>
#include <string>

#include "flags.h"

namespace {
   const FlagSpec FLAG_SPECS[] = {
      { 'a', "leave-asm",  Flags::LEAVE_ASM,       false, "Keep the generated .asm file." },
      { 'j', "leave-obj",  Flags::LEAVE_OBJ,       false, "Keep the generated .o file." },
      { 's', "ast",        Flags::PRINT_AST,       false, "Print the abstract syntax tree." },
      { 'f', "flags",      Flags::PRINT_FLAGS,     false, "Print the flags that were passed in." },
      { 't', "tokens",     Flags::PRINT_TOKENS,    false, "Print the lexer's tokens." },
      { 'p', "keep-preop", Flags::PRESERVE_PRE_OP, false, "Keep the pre-optimization .asm" },
      { 'o', "output",     Flags::USER_NAME,       true,  "Set the output program name (-o <name>)" },
      { 'h', "help",       Flags::HALLLLLPUH,      false, "Print this help and exit." },
      { 'd', "debug",      Flags::DEBUG,           false, "Logs every action for debugging." },
   };


   const FlagSpec* find_short(char c) {
      for (const auto& s : FLAG_SPECS)
         if (s.short_name == c) return &s;
      return nullptr;
   }
   const FlagSpec* find_long(const std::string& name) {
      for (const auto& s : FLAG_SPECS)
         if (s.long_name && name == s.long_name) return &s;
      return nullptr;
   }
}


void CommandLine::print_help(const char* prog_name) {
   std::cout << "Usage: " << prog_name << " [options] <file.z>\n\n"
             << "Options:\n";
   for (const auto& s : FLAG_SPECS) {
      // format: -t, --tokens   help
      std::string left = "  ";
      if (s.short_name) { left += '-'; left += s.short_name; }
      else              { left += "  "; }
      if (s.long_name)  { left += ", --"; left += s.long_name; }
      if (s.takes_value) left += " <value>";
      // pad to a column
      while (left.size() < 28) left += ' ';
      std::cout << left << s.help << "\n";
   }
   std::cout << std::endl;
}


std::optional<ParsedArgs> CommandLine::parse(int argc, char* argv[]) {
   const char* prog = argv[0];

   if (argc < 2) {
      std::cerr << "Incorrect usage.\n";
      CommandLine::print_help(prog);
      return std::nullopt;
   }

   ParsedArgs args;
   bool have_input = false;

   for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];

      if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
         std::string body = arg.substr(2);
         std::string value;
         bool has_inline_value = false;

         if (auto eq = body.find('='); eq != std::string::npos) {
            value = body.substr(eq + 1);
            body  = body.substr(0, eq);
            has_inline_value = true;
         }

         const FlagSpec* spec = find_long(body);
         if (!spec) {
            std::cerr << "Unknown flag: --" << body << "\n";
            return std::nullopt;
         }

         if (spec->flag == Flags::HALLLLLPUH) { CommandLine::print_help(prog); return std::nullopt; }
         args.flags.push_back(spec->flag);

         if (spec->takes_value) {
            if (has_inline_value) {
               args.output_name = value;
            } else if (i + 1 < argc) {
               args.output_name = argv[++i];
            } else {
               std::cerr << "Flag --" << body << " requires a value.\n";
               return std::nullopt;
            }
         }
         continue;
      }


      if (arg.size() >= 2 && arg[0] == '-') {
         for (size_t j = 1; j < arg.size(); j++) {
            const FlagSpec* spec = find_short(arg[j]);
            if (!spec) {
               std::cerr << "Unknown flag: -" << arg[j] << "\n";
               return std::nullopt;
            }

            if (spec->flag == Flags::HALLLLLPUH) { CommandLine::print_help(prog); return std::nullopt; }

            args.flags.push_back(spec->flag);

            if (spec->takes_value) {
               if (j + 1 < arg.size()) {
                  args.output_name = arg.substr(j + 1);
                  break;
               } else if (i + 1 < argc) {
                  args.output_name = argv[++i];
               } else {
                  std::cerr << "Flag -" << spec->short_name << " requires a value.\n";
                  return std::nullopt;
               }
            }
         }
         continue;
      }

      if (have_input) {
         std::cerr << "Multiple input files given; only one is supported at the moment.\n";
         return std::nullopt;
      }
      args.input_file = arg;
      have_input = true;
   }

   if (!have_input) {
      std::cerr << "No input file given.\n";
      CommandLine::print_help(prog);
      return std::nullopt;
   }

   if (!args.input_file.ends_with(".z")) {
      std::cerr << "Unrecognized file \"" << args.input_file
                << "\": expected a .z source file.\n";
      return std::nullopt;
   }

   return args;
}