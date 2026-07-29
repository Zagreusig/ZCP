#ifndef LOG_H
#define LOG_H

#include <string>

#include "utils/phase.h"

class Logger;
enum class CompPhase;

namespace Log {
   void set_sink(Logger* logger);
   Logger* sink();

   void trace(CompPhase cat, std::string msg, std::string file = "", int line = 0, int col = 0);
   void info (CompPhase cat, std::string msg, std::string file = "", int line = 0, int col = 0);
   void warn (CompPhase cat, std::string msg, std::string file = "", int line = 0, int col = 0);
   void error(CompPhase cat, std::string msg, std::string file = "", int line = 0, int col = 0);
}

#endif // LOG_H