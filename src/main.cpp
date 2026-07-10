#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "utils/file_util.h"
#include "driver/CommandLine.h"
#include "driver/compiler.h"

int main(int argc, char* argv[]) {
   auto args = CommandLine::parse(argc, argv);
   if (!args) return EXIT_FAILURE;

   auto source = read_file(args->input_file);
   if (!source) return EXIT_FAILURE;

   Compiler ctx(*source, args->flags, args->output_name, args->input_file);
   return ctx.run();
}