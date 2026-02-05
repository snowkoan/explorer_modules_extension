#include "Log.h"

#include <strsafe.h>
#include <cstdarg>

namespace Log {
namespace {
const wchar_t* LevelToString(Level level) {
    switch (level) {
    case Level::Trace:
        return L"TRACE";
    case Level::Info:
        return L"INFO";
    case Level::Warn:
        return L"WARN";
    case Level::Error:
        return L"ERROR";
    default:
        return L"LOG";
    }
}
} // namespace

void Write(Level level, const wchar_t* format, ...) {
    if (!format) {
        return;
    }

    wchar_t message[1024] = {};
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);

    wchar_t output[1200] = {};
    StringCchPrintfW(output, ARRAYSIZE(output), L"[ExplorerModules][%s] %s\n", LevelToString(level), message);
    OutputDebugStringW(output);
}
} // namespace Log
