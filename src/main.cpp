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

   std::vector<Flags> flags {};
   std::string user_name = "out";
   if (argc > 2) {
      for (int i = 1; i < argc - 1; i++) {
         if (argv[i][strlen(argv[i])] == '.' && 
             argv[i][strlen(argv[i]) + 1] == 'z') { break; }
         switch (argv[i][1]) {
            case 'a': flags.push_back(Flags::LEAVE_ASM);    break;
            case 'j': flags.push_back(Flags::LEAVE_OBJ);    break;
            case 's': flags.push_back(Flags::PRINT_AST);    break;
            case 'f': flags.push_back(Flags::PRINT_FLAGS);  break;
            case 't': flags.push_back(Flags::PRINT_TOKENS); break;
            case 'o': flags.push_back(Flags::USER_NAME); 
                      user_name = argv[i + 1]; break;
            default:
               if (flags.back() == Flags::USER_NAME) continue;
               std::cerr << "Unknown flag: " << argv[i] << std::endl;
               return EXIT_FAILURE;
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