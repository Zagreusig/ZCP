#include "utils/flags.h"
#include "utils/syscaller.h"
#include "lexer/lexer.h"
#include "codegen/generation.h"  
#include "parser/parser.h"
#include "ErrAndRep/ErrorHandler.h"
#include "analyzer/analyer.h"
#include "optimizations/optimizer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>


int main(int argc, char* argv[]) {
   if (argc < 2) {
      std::cerr << "Incorrect usage." << std::endl;
      std::cerr << "Usage as follows: ./zcp <name.z>" << std::endl;
      return EXIT_FAILURE;
   }
   std::string input_file = argv[argc - 1];
   if (!input_file.contains(".z")) {
      std::cerr << "Unrecognized file." << std::endl;
      std::cerr << "Call format: ./zcp <name.z>" << std::endl;
      return EXIT_FAILURE;
   }
   {
      std::fstream file(input_file);
      if (!file.is_open()) {
         std::cerr << "Could not find file \"" << input_file << "\".\n";
         return EXIT_FAILURE;
      }
   }
   std::vector<Flags> flags {};
   std::string user_name = "out";
   if (argc > 2) {
      for (int i = 1; i < argc; i++) {
         std::string flag = argv[i];
         if (flag.contains(".z")) { break; }

         for (size_t j = 1; j < flag.length(); j++) {
            switch (flag.at(j)) {
               case 'a': flags.push_back(Flags::LEAVE_ASM);       continue;
               case 'j': flags.push_back(Flags::LEAVE_OBJ);       continue;
               case 's': flags.push_back(Flags::PRINT_AST);       continue;
               case 'f': flags.push_back(Flags::PRINT_FLAGS);     continue;
               case 't': flags.push_back(Flags::PRINT_TOKENS);    continue;
               case 'p': flags.push_back(Flags::PRESERVE_PRE_OP); continue;
               case 'o': flags.push_back(Flags::USER_NAME); 
                        user_name = argv[i++ + 1];                continue;
               case 'h':
                  std::cout << "Help!!\n"
                           << "-a        Leaves assembly file\n"
                           << "-j        Leaves object file\n"
                           << "-s        Prints Asymmetric Syntax Tree\n"
                           << "-f        Prints flags passed in \n"
                           << "-t        Prints the Lexer's tokens\n"
                           << "-o {name} Determines the name of the program\n"
                           << "-h        Prints this list." << std::endl;
                  break;
               default:
                  if (flags.back() == Flags::USER_NAME) continue;
                  std::cerr << "Unknown flag: " << flag.at(j) << std::endl;
                  return EXIT_FAILURE;
            }
         }
      }
   }

   std::string contents;
   std::stringstream in_stream;
   {
      std::fstream input(argv[argc - 1], std::ios::in);
      in_stream << input.rdbuf();
      contents = in_stream.str();
   }

   Diagnostics diag;

   // file -> Tokens
   Lexer Lexer(contents);
   std::vector<Token> tokens = Lexer.tokenize(&diag);
   
   // Tokens -> AST
   Parser parser(tokens, &diag);
   auto prog = parser.parse_prog();
   if (diag.has_errors()) {
      diag.report_all(contents, input_file);
      Syscaller err;
      err.give_toks(tokens); err.set_flags(flags);

      err.make_calls(true);
      return EXIT_FAILURE;
   }

   // Make sure everythings in order
   Analyzer analyzer(prog.value(), diag);
   analyzer.analyze();
   if (diag.has_errors()) { diag.report_all(contents, input_file); exit(EXIT_FAILURE); }

   // Generate assembly!
   Syscaller calls(user_name, tokens, flags, prog.value());
   ASMGenerator ASMGenerator(prog.value());
   std::string temp = ASMGenerator.build();
   Optimizer optimizer(temp);
   optimizer.optimize();
   {
      std::fstream file(calls.get_name() + ".asm", std::ios::out);
      std::fstream other("original.txt", std::ios::out);
      file << optimizer.finish();
      other << optimizer.orig();
   }
   calls.make_calls(diag.has_errors());

   return EXIT_SUCCESS;
}