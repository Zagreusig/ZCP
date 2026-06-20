#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <stdexcept>
#include <string>
#include <vector>

enum class CompPhase { Lexing, Parsing, CodeGen };

class CompilerError : public std::runtime_error {
public:
   CompilerError(CompPhase phase, const std::string& msg)
      : std::runtime_error(msg), m_phase(phase) {}
   
   CompPhase phase() const { return m_phase; }
   inline int line() const { return m_line; }
   inline int col()  const { return m_col; }


private:
   CompPhase m_phase;
   int m_line, m_col;
};

void report(const std::string&, const std::string&, const CompilerError&);
std::string fetch_line(const std::string&, int);

struct Diagnostic {
   CompPhase phase;
   int line, col;
   std::string msg;
};

class Diagnostics {
public:
   void error (CompPhase phase, int line, int col, const std::string& msg) {
      m_list.push_back({ phase, line, col, msg });
   }
   bool has_errors() const { return !m_list.empty(); }
   size_t count() const { return m_list.size(); }

   void report_all(const std::string&, const std::string&) const;
private:
   void report_one(const std::string&, const std::string&, const Diagnostic&) const;

   std::vector<Diagnostic> m_list;
};


#endif // ERRORHANDLER_H