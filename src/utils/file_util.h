#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

inline std::optional<std::string> read_file(const std::string& path) {
   std::ifstream input(path);
   if (!input.is_open()) {
      std::cerr << "Could not open the file provided." << std::endl;
      return std::nullopt;
   }
   
   std::stringstream ss;
   ss << input.rdbuf();
   return ss.str();
}

inline void write_file(const std::string& path, const std::string& output, auto mode = std::ios::out) {
   std::ofstream outfile(path, mode);
   if (!outfile.is_open()) {
      return;
   }

   outfile << output;
}

#endif // FILE_UTIL_H