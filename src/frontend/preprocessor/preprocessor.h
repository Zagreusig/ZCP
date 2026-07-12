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
*  { "preproc.h": { {"Preprocessor", PREPROC::CLASS, <obj>}, {"BIT", PREPROC::MACRO, <obj>}, { "BANG", PREPROC::MACRO, <obj>}}}
* }
*
*
* IFNDEF THIS_H <- macro
* Find the matching .z file, 
*/

enum class PreprocessorDirectives { INCLUDE, IFNDEF, DEF, UNDEF, PRAGMA, IF, ELIF, FI, STRTOK, CATTOK, ERR, WARN };

class Preprocessor {
public:
   Preprocessor() {}

private:

};

#endif // PREPROCESSOR_H