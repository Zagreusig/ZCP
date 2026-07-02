#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class CompPhase { Lexing, Parsing, Analysis, CodeGen };

static std::string str(CompPhase phase) {
   switch (phase) {
      case CompPhase::Lexing:   return "Lexing";
      case CompPhase::Parsing:  return "Parsing";
      case CompPhase::Analysis: return "Analysis";
      case CompPhase::CodeGen:  return "CodeGen";
      default:                  return "Null";
   }
}

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
      m_errors[phase].push_back({ phase, line, col, msg });
      err_count++;
   }
   bool has_errors() const { return !m_errors.empty(); }
   size_t count() const { return m_errors.size(); }

   void report_all(const std::string&, const std::string&) const;
private:
   void report_one(const std::string&, const std::string&, const Diagnostic&) const;

   std::unordered_map<CompPhase, std::vector<Diagnostic>> m_errors;
   long unsigned int err_count = 0;
};


#endif // ERRORHANDLER_H