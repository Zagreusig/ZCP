#include "lexer/lexer.h"
#include "codegen/generation.h"  
#include "parser/parser.h"
#include <iostream>
#include <fstream>
#include <sstream>


int main(int argc, char* argv[]) {
   if (argc < 2) {
      std::cerr << "Incorrect usage." << std::endl;
      std::cerr << "Usage as follows: rxr <name.rx>" << std::endl;
      return EXIT_FAILURE;
   }

   std::string contents;
   std::stringstream in_stream;
   {
      std::fstream input(argv[1], std::ios::in);
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
   {
      std::fstream file("out.asm", std::ios::out);
      file << ASMGenerator.build();
   }

   system("nasm -felf64 out.asm");
   if (argc == 3) {
      std::string linker= "ld -o ";
      linker += argv[2];
      linker += " out.o";
      system(linker.c_str());
   }
   else system("ld -o out out.o");
   // system("rm *.o *.asm");

   return EXIT_SUCCESS;
}