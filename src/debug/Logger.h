#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <iosfwd>
#include <utility>

#include "utils/phase.h"

enum class Severity { Trace, Info, Warn, Error };

inline const char* to_string(Severity s) {
   switch (s) {
      case Severity::Trace: return "TRACE";
      case Severity::Info:  return "INFO";
      case Severity::Warn:  return "WARN";
      case Severity::Error: return "ERROR";
      default:              return "?????";
   }
}

struct LogEntry {
   CompPhase   phase;
   Severity    severity;
   std::string message;
   int         line = 0; // 0 == no location
   int         col  = 0;
};

class Logger {
public:
   using Clock    = std::chrono::steady_clock;
   using Duration = std::chrono::nanoseconds;

   Logger() = default;

   void enable(bool on) { m_enabled = on; }
   bool enabled() const { return m_enabled; }

   // -- q entries --
   void log(CompPhase phase, Severity sev, std::string msg, int line = 0, int col = 0) {
      if (!m_enabled) return;
      m_entries.push_back(LogEntry{ phase, sev, std::move(msg), line, col });
   }

   // wrappers :]
   void trace(CompPhase p, std::string m, int l = 0, int c = 0);
   void info (CompPhase p, std::string m, int l = 0, int c = 0);
   void warn (CompPhase p, std::string m, int l = 0, int c = 0);
   void error(CompPhase p, std::string m, int l = 0, int c = 0);

   // timing
   void record_time(CompPhase phase, Duration d) {
      if (!m_enabled) return;
      m_times[phase] += d;
   }

   void record_passes(int passes) { m_opt_passes = passes; }
   
   // failure tracking
   void mark_phase(CompPhase p)  { m_last_phase = p; }
   void mark_failed(CompPhase p) { m_failed = true; m_failed_phase = p; }
   bool failed() const { return m_failed; }

   // dumps things -> into log
   void set_flags(std::string s)    { m_flags       = std::move(s); }
   void set_tokens(std::string s)   { m_tokens_dump = std::move(s); }
   void set_ast(std::string s)      { m_ast_dump    = std::move(s); }
   void set_orig_asm(std::string s) { m_orig_asm    = std::move(s); }
   void set_opt_asm(std::string s)  { m_opt_asm     = std::move(s); }

   // -- rendering --
   // writes the log.
   void flush(std::ostream& out) const;

   // Convinience: open path & flush it. Return false on open failure.
   bool flush_to_file(const std::string& path) const;

private:
   bool m_enabled = false;

   std::vector<LogEntry> m_entries;
   std::unordered_map<CompPhase, Duration> m_times;

   int m_opt_passes = 0;
   bool m_failed = false;
   CompPhase m_failed_phase = CompPhase::Lexing;
   CompPhase m_last_phase   = CompPhase::Lexing;

   std::string m_flags, m_tokens_dump, m_ast_dump, m_orig_asm, m_opt_asm;
};

// ============================================================================
//  ScopedPhaseTimer — RAII. Records elapsed time on scope exit, even if the
//  phase throws CompilerError (destructors run during unwinding).
// ============================================================================
class ScopedPhaseTimer {
public:
   ScopedPhaseTimer(Logger& log, CompPhase phase)
      : m_log(log), m_phase(phase), m_start(Logger::Clock::now()) {
      m_log.mark_phase(phase);
   }
   ~ScopedPhaseTimer() {
      m_log.record_time(m_phase, Logger::Clock::now() - m_start);
   }
   ScopedPhaseTimer(const ScopedPhaseTimer&) = delete;
   ScopedPhaseTimer& operator=(const ScopedPhaseTimer&) = delete;

private:
   Logger& m_log;
   CompPhase m_phase;
   Logger::Clock::time_point m_start;
};

#endif // LOGGER_H