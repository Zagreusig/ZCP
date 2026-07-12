#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

/**
* include import with
* # .. \\
* define(def) undef ifndef
* pragma once
* if elif else fi
* error warning
* # turns macro to string
* ## concats macro toks
* <> "" for files
* <> system, "" use current dir
*/

/**
* Lexer -> preprocessor -> parser ...
* Parse all of the headers for definitions, create a sort of global table of definitions
* unordered map?
*
*
* class Preprocessor
* #define BIT 1
* #define BANG(x) !x
* SYMBOLS = {
*  { "this header": { list, of, definitions, here, lol }},
*  { "preproc.h": { {"Preprocessor", PREPROC::CLASS, false}, {"BIT", PREPROC::MACRO, false}, { "BANG", PREPROC::MACRO, true}}}
* }
*/
class Preprocessor {
public:
   Preprocessor() {}

private:

};

#endif // PREPROCESSOR_H