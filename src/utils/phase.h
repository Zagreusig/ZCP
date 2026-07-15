#ifndef PHASE_H
#define PHASE_H

#include <stdexcept>
#include <string>

enum class CompPhase { Lexing, Preprocessing, Parsing, Analysis, CodeGen, Optimization };

inline std::string phase_str(CompPhase phase) {
   switch (phase) {
      case CompPhase::Lexing:        return "Lexing";
      case CompPhase::Preprocessing: return "Preprocessing";
      case CompPhase::Parsing:       return "Parsing";
      case CompPhase::Analysis:      return "Analysis";
      case CompPhase::CodeGen:       return "CodeGen";
      case CompPhase::Optimization:  return "Optimization";
      default:                       return "Null";
   }
}

class CompilerError : public std::runtime_error {
public:
   CompilerError(CompPhase phase, int line, int col, const std::string& msg)
      : std::runtime_error(msg), m_phase(phase), m_line(line), m_col(col) {}
   CompPhase phase() const { return m_phase; }
   inline int line() const { return m_line; }
   inline int col()  const { return m_col; }

private:
   CompPhase m_phase;
   int m_line = 0;
   int m_col = 0;
};

struct Diagnostic {
   CompPhase phase;
   std::string file_name;
   int line, col;
   std::string msg;
};

#endif // PHASE_H