#pragma once

#include <windows.h>

namespace Log {
enum class Level {
    Critical,
    Error,
    Warn,
    Info,
    Trace    
};

// Compile-time configuration for log level
constexpr Level kMaxLogLevel = Level::Info;

void Write(Level level, const wchar_t* format, ...);
} // namespace Log
