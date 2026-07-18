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


struct SourceFile {
   std::string path;
   std::string source;
};


class Diagnostics {
public:
   void attach_logger(Logger* log) { m_log = log; }
   void set_phase(CompPhase phase) { m_phase = phase; }

   void error (CompPhase phase, const std::string& file_name, int line, int col, const std::string& msg) {
      m_errors[phase].push_back({ phase, file_name, line, col, msg });
      err_count++;
      if (m_log) m_log->error(phase, msg, file_name, line, col);
   }
   void error (const std::string& file_name, int line, int col, const std::string& msg) {
      error(m_phase, file_name, line, col, msg);
   }

   [[noreturn]] void fatal (CompPhase phase, const std::string& file_name, int line, int col, const std::string& msg) {
      error(phase, file_name, line, col, msg);
      if (m_log) m_log->mark_failed(phase);
      throw CompilerError(phase, line, col, msg);
   }
   [[noreturn]] void fatal (const std::string& file_name, int line, int col, const std::string& msg) {
      fatal(m_phase, file_name, line, col, msg);
   }

   void warn (CompPhase phase, const std::string& file_name, int line, int col, const std::string& msg) {
      m_warns[phase].push_back({ phase, file_name, line, col, msg });
      warn_count++;
      if (m_log) m_log->warn(phase, msg, file_name, line, col);
   }
   void warn(const std::string& file_name, int line, int col, const std::string& msg) {
      warn(m_phase, file_name, line, col, msg);
   }

   bool has_errors() const { return !m_errors.empty(); }
   size_t count() const { return m_errors.size(); }

   void report_all(const std::string&) const;
private:
   void report_one(const std::string&, const std::string&, const Diagnostic&) const;
   void report_one_warn(const std::string&, const std::string&, const Diagnostic&) const;

   std::unordered_map<CompPhase, std::vector<Diagnostic>> m_errors;
   long unsigned int err_count = 0;

   std::unordered_map<CompPhase, std::vector<Diagnostic>> m_warns;
   long unsigned int warn_count = 0;

   CompPhase m_phase;

   Logger* m_log = nullptr;
};


#endif // ERRORHANDLER_H