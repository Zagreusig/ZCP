#include "Log.h"

#include <cstdio>

#include "Logger.h"

enum class CompPhase;

namespace {
   Logger* g_sink = nullptr;
}


void Log::set_sink(Logger* logger) { g_sink = logger; }
Logger* Log::sink() { return g_sink; }

void Log::trace(CompPhase cat, std::string msg, std::string file, int line, int col) {
   if (g_sink) g_sink->trace(cat, msg, file, line, col);
}
void Log::info(CompPhase cat, std::string msg, std::string file, int line, int col) {
   if (g_sink) g_sink->info(cat, msg, file, line, col);
}
void Log::warn(CompPhase cat, std::string msg, std::string file, int line, int col) {
   if (g_sink) g_sink->warn(cat, msg, file, line, col);
}
void Log::error(CompPhase cat, std::string msg, std::string file, int line, int col) {
   // Always visible, regardless of -d: this is the only error-reporting
   // channel lexer/parser have (they don't hold a Diagnostics reference by
   // design, to stay decoupled from Compiler). Diagnostics-routed errors
   // (analyzer) print separately via Diagnostics::report_all()'s caret
   // rendering, so this isn't a duplicate for that path.
   if (line > 0)
      fprintf(stderr, "\x1b[1m%s:%d:%d: \x1b[31merror: \x1b[0m %s\n", file.c_str(), line, col, msg.c_str());
   else
      fprintf(stderr, "\x1b[31merror: \x1b[0m %s\n", msg.c_str());
   if (g_sink) g_sink->error(cat, msg, file, line, col);
}