#include "Logger.h"

#include <fstream>
#include <iomanip>
#include <sstream>

void Logger::trace(CompPhase p, std::string m, int l, int c) {
   log(p, Severity::Trace, std::move(m), l, c);
}


void Logger::info(CompPhase p, std::string m, int l, int c) {
   log(p, Severity::Info, m, l, c);
}


void Logger::warn(CompPhase p, std::string m, int l, int c) {
   log(p, Severity::Warn, m, l, c);
}


void Logger::error(CompPhase p, std::string m, int l, int c) {
   log(p, Severity::Error, m, l, c);
}

namespace {

// readable durations: ms / s (us for very fast)
std::string fmt_duration(Logger::Duration d) {
   using namespace std::chrono;
   auto ns = d.count();

   std::ostringstream ss;
   ss << std::fixed << std::setprecision(3);

   if (ns >= 1'000'000'000LL)  ss << (ns / 1e9) << " s";
   else if (ns >= 1'000'000LL) ss << (ns / 1e6) << " ms";
   else if (ns >= 1'000LL)     ss << (ns / 1e3) << " us";
   else                        ss << ns << " ns";
   return ss.str();
}


void rule(std::ostream& out) {
   out << "\n" << std::string(72, '-') << "\n\n";
}

} // namespace


void Logger::flush(std::ostream& out) const {
   // 1. trace entries, ordered
   out << "=== COMPILATION LOG ===\n\n";
   for (const auto& e : m_entries) {
      // "Lexing: [ERROR] Missing ';' 1:14"
      out << phase_str(e.phase) << ": ";

      // pad for message lineup
      for (size_t i = phase_str(e.phase).size(); i < 11; i++) out << ' ';
      out << "[" << to_string(e.severity) << "] " << e.message;
      if (e.line > 0) out << "  " << e.line << ':' << e.col; 
      out << '\n';
   }

   // 2. failure marker
   if (m_failed) {
      out << "\nFailed on phase: \"" << phase_str(m_failed_phase) << "\"\n";
   } else {
      out << "\nCompleted all phases successfully.\n";
   }

   // 3. Dumps
   if (!m_flags.empty()) {
      rule(out);
      out << "FLAGS\n\n" << m_flags << "\n";
   }
   if (!m_tokens_dump.empty()) {
      rule(out);
      out << "TOKENS\n\n" << m_tokens_dump << "\n";
   }
   if (!m_ast_dump.empty()) {
      rule(out);
      out << "AST\n\n" << m_ast_dump << "\n";
   }
   if (!m_orig_asm.empty()) {
      rule(out);
      out << "ORIGINAL ASM\n\n" << m_orig_asm << "\n";
   }
   if (!m_opt_asm.empty()) {
      rule(out);
      out << "OPTIMIZED ASM\n\n" << m_opt_asm << "\n";
   }

   // 4. Timing summary
   rule(out);
   out << "TIME PER PHASE\n\n";

   // Print in pipeline order, not hash order
   const CompPhase order[] = {
      CompPhase::Lexing, CompPhase::Parsing, CompPhase::Analysis,
      CompPhase::CodeGen, CompPhase::Optimization
   };

   Duration total(0);
   for (CompPhase p : order) {
      auto it = m_times.find(p);
      if (it == m_times.end()) continue;
      total += it->second;

      std::string name = phase_str(p);
      out << "  " << name << ":";
      for (size_t i = name.size(); i < 13; i++) out << " ";
      out << fmt_duration(it->second);

      if (p == CompPhase::Optimization && m_opt_passes > 0)
         out << ", " << m_opt_passes << " pass" << (m_opt_passes == 1 ? "" : "es");
      out << "\n";
   }

   out << "\n Total:         " << fmt_duration(total) << "\n";
   out << std::endl;
}


bool Logger::flush_to_file(const std::string& path) const {
   if (!m_enabled) return true; // nothing to write, not error
   std::ofstream f(path);
   if (!f.is_open()) return false;
   flush(f);
   return true;
}