#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <stddef.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils/phase.h"
#include "debug/Logger.h"

void report(const std::string&, const std::string&, const CompilerError&);
std::string fetch_line(const std::string&, int);

class Diagnostics {
public:
   void attach_logger(Logger* log) { m_log = log; }

   void error (CompPhase phase, int line, int col, const std::string& msg) {
      m_errors[phase].push_back({ phase, line, col, msg });
      err_count++;
      if (m_log) m_log->error(phase, msg, line, col);
   }

   [[noreturn]] void fatal (CompPhase phase, int line, int col, const std::string& msg) {
      error(phase, line, col, msg);
      if (m_log) m_log->mark_failed(phase);
      throw CompilerError(phase, line, col, msg);
   }
   bool has_errors() const { return !m_errors.empty(); }
   size_t count() const { return m_errors.size(); }

   void report_all(const std::string&, const std::string&) const;
private:
   void report_one(const std::string&, const std::string&, const Diagnostic&) const;

   std::unordered_map<CompPhase, std::vector<Diagnostic>> m_errors;
   long unsigned int err_count = 0;

   Logger* m_log = nullptr;
};


#endif // ERRORHANDLER_H