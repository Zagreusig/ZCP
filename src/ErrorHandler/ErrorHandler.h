#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <stdexcept>
#include <string>

enum class CompPhase { Lexing, Parsing, CodeGen };

class CompilerError : public std::runtime_error {
public:
   CompilerError(CompPhase phase, const std::string& msg)
      : std::runtime_error(msg), m_phase(phase) {}
   CompPhase phase() const { return m_phase; }

private:
   CompPhase m_phase;
};


#endif // ERRORHANDLER_H