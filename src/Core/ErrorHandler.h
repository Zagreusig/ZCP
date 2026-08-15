#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <cxxabi.h>
#include <stddef.h>
#include <cstdlib>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <typeinfo>

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


/**
* This helps with debugging variant visits & gets :D
* because seing "'what()': std::bad_variant_access" is pmo
*/
struct BadVariantAccess : std::runtime_error {
   BadVariantAccess(const char* expected, size_t held_index, std::source_location loc)
      : std::runtime_error(build_msg(expected, held_index, loc)) {}

      private:
   inline std::string build_msg(const char* expected, size_t held_index, std::source_location loc) {
      return "\nbad variant access: expected " + demangle(expected) +
             ", held index " + std::to_string(held_index) +
             ", at " + loc.file_name() + ":" + std::to_string(loc.line()) +
             " in " + loc.function_name();
   }
   std::string demangle(const char* mangled) const {
      int status = 0;
      std::unique_ptr<char, decltype(&std::free)> buf (
         abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
      return (status == 0) ? std::string(buf.get()) : std::string(mangled);
   }
};

template<typename T, typename Variant>
decltype(auto) variant_get(const Variant& v, std::source_location loc = std::source_location::current()) {
   if (auto* p = std::get_if<T>(&v)) return *p;
   throw BadVariantAccess(typeid(T).name(), v.index(), loc);
}


#endif // ERRORHANDLER_H