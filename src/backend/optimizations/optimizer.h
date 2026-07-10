#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <sstream>
#include <string>

class Optimizer {
public:
   Optimizer(std::string generated) 
      : m_orig(generated) {}

   void optimize();
   std::string peephole();

   // DEBUG
   std::string orig()   { return m_orig; }
   std::string finish() { return m_optimized; }
   int passes()         { return m_passes; }

private:
   bool is_push(const std::string&, std::string&);
   bool is_pop(const std::string&, std::string&);
   bool is_register(const std::string&);
   bool is_noop(const std::string&);
   bool is_cond(const std::string&);
   bool is_ret(const std::string&);
   bool is_boundary(const std::string&);
   bool try_to_xor(const std::string&, std::string&);
   bool same_xor(const std::string&, std::string&);

   void trim(std::string&);

   int         m_passes = 0;
   std::string m_orig;
   std::string m_optimized;
};

#endif // OPTIMIZER_H