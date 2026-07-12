#ifndef SYSCALLER_H
#define SYSCALLER_H

#include <stdexcept>
#include <string>
#include <utility>

class Syscaller {
public:
   struct Options {
      bool keep_asm   = false;
      bool keep_obj   = false;
      bool keep_preop = false;
      bool debug_enbl = false;
   };

   Syscaller() {}
   Syscaller(std::string prog_name, Options opts)
      : m_name(std::move(prog_name)), m_opts(opts) {
         m_asm = m_name + ".asm";
         m_obj = m_name + ".o";
      }

   int assemble_and_link() {
      if (!nasm())   throw std::runtime_error("Nasm failure");
      if (!linker()) throw std::runtime_error("ld failure.");
      cleanup();
      return 0;
   }

   private:
      bool nasm();
      bool linker();
      void cleanup();

      std::string m_name = {}, m_asm = {}, m_obj = {};
      Options m_opts;
};

#endif // SYSCALLER_H