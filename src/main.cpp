#include "utils/flags.h"
#include "utils/syscaller.h"
#include "lexer/lexer.h"
#include "codegen/generation.h"  
#include "parser/parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>


int main(int argc, char* argv[]) {
   if (argc < 2) {
      std::cerr << "Incorrect usage." << std::endl;
      std::cerr << "Usage as follows: zcp <name.z>" << std::endl;
      return EXIT_FAILURE;
   }
   std::string input_file = argv[argc - 1];
   if (!input_file.contains(".z")) {
      std::cerr << "Unrecognized file." << std::endl;
      std::cerr << "Must end in '.z'" << std::endl;
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
               case 'a': flags.push_back(Flags::LEAVE_ASM);    continue;
               case 'j': flags.push_back(Flags::LEAVE_OBJ);    continue;
               case 's': flags.push_back(Flags::PRINT_AST);    continue;
               case 'f': flags.push_back(Flags::PRINT_FLAGS);  continue;
               case 't': flags.push_back(Flags::PRINT_TOKENS); continue;
               case 'o': flags.push_back(Flags::USER_NAME); 
                        user_name = argv[i++ + 1]; continue;
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

   Lexer Lexer(std::move(contents));
   std::vector<Token> tokens = Lexer.tokenize();

   Parser parser(std::move(tokens));

   std::optional<NodeProg> prog = parser.parse_prog();
   if (!prog.has_value())  {
      std::cerr << "No exit statement found." << std::endl;
      exit(EXIT_FAILURE);
   } 
   


   ASMGenerator ASMGenerator(prog.value());
   Syscaller calls(user_name, parser.regurg_toks(), flags, prog.value());
   {
      std::fstream file(calls.get_name() + ".asm", std::ios::out);
      file << ASMGenerator.build();
   }

   calls.make_calls();

   return EXIT_SUCCESS;
}