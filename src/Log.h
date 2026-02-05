#pragma once

#include <windows.h>

namespace Log {
enum class Level {
    Trace,
    Info,
    Warn,
    Error
};

void Write(Level level, const wchar_t* format, ...);
} // namespace Log
