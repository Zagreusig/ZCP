#include "Log.h"

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
   if (g_sink) g_sink->error(cat, msg, file, line, col);
}