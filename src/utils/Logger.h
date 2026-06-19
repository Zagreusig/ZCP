#ifndef LOGGER_H
#define LOGGER_H

#ifdef _WIN32
   #define DEBUG_BREAK() __debugbreak()
#elif __linux__
   #define DEBUG_BREAK() __builtin_debugtrap()
#elif __APPLE__
   #define DEBUG_BREAK() __builtin_trap()
#else
   #define DEBUG_BREAK()
#endif

#include <stdio.h>

enum TextColor 
{
   TEXT_COLOR_RED,
   TEXT_COLOR_GREEN,
   TEXT_COLOR_YELLOW,
   TEXT_COLOR_WHITE,
   TEXT_COLOR_COUNT
};

template <typename ...Args>
void _log(const char* prefix, const char* msg, TextColor textColor, Args... args) 
{
   static const char* TextColorTable[TEXT_COLOR_COUNT] =
   {
      "\x1b[31m", // RED
      "\x1b[32m", // GREEN
      "\x1b[33m", // YELLOW
      "\x1b[37m", // WHITE
   };

   char formatBuffer[8192] = {};
   snprintf(formatBuffer, sizeof(formatBuffer), "%s %s %s \033[0m", 
            TextColorTable[textColor], prefix, msg);

   char textBuffer[8192] = {};
   snprintf(textBuffer, sizeof(textBuffer), formatBuffer, args...);

   puts(textBuffer);
}

#define ZL_TRACE(msg, ...)                                \
do                                                        \
{                                                         \
   _log("TRACE: ", msg, TEXT_COLOR_GREEN, ##__VA_ARGS__); \
} while(0)

#define ZL_WARN(msg, ...)                                 \
do                                                        \
{                                                         \
   _log("WARN: ", msg, TEXT_COLOR_YELLOW, ##__VA_ARGS__); \
} while(0)

#define ZL_ERROR(msg, ...)                              \
do                                                      \
{                                                       \
   _log("ERROR: ", msg, TEXT_COLOR_RED, ##__VA_ARGS__); \
} while(0)

#define ZL_ASSERT(x, msg, ...)      \
do                                  \
{                                   \
   if (!(x))                        \
   {                                \
      ZL_ERROR(msg, ##__VA_ARGS__); \
      DEBUG_BREAK();                \
   }                                \
} while(0)

#endif // LOGGER_H