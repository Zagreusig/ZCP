#include "ErrorHandler.h"

void report(const std::string& src, const std::string& file, const CompilerError& e) {
   fprintf(stderr, "\x1b[1m%s:%d:%d: \x1b[31merror: \x1b[0m %s\n",
           file.c_str(), e.line(), e.col(), e.what());

   std::string line = fetch_line(src, e.line());
   fprintf(stderr, "   %s\n", line.c_str());

   fprintf(stderr, "   %*s^\n", e.col() - 1, "");
}


std::string fetch_line(const std::string& src, int line) {
   int curr = 1;
   size_t start = 0;
   for (size_t i = 0; i <= src.size(); i++) {
      if (i == src.size() || src[i] == '\n') {
         if (curr == line)
            return src.substr(start, i - start);
         curr++;
         start = i + 1;
      }
   }
   return ""; // line out of range / not found
}


void Diagnostics::report_all(const std::string& src, const std::string& file) const {
   for (const auto& item : m_list)
      report_one(src, file, item);
   if (!m_list.empty())
      fprintf(stderr, "\n%zu error%s generated.\n",
              m_list.size(), m_list.size() == 1 ? "" : "s");
}


void Diagnostics::report_one(const std::string& src, const std::string& file, const Diagnostic& diag) const {
   fprintf(stderr, "\x1b[1m%s:%d:%d: \x1b[31merror: \x1b[0m %s\n",
           file.c_str(), diag.line, diag.col, diag.msg.c_str());

   std::string line = fetch_line(src, diag.line);
   fprintf(stderr, "   %s\n", line.c_str());
   fprintf(stderr, "   %*s\x1b[32m^\x1b[0m\n", diag.col > 0 ? diag.col - 1 : 0, "");
}