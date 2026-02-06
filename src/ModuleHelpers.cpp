#include "ModuleHelpers.h"
#include "Log.h"
#include <windows.h>
#include <psapi.h>
#include <strsafe.h>

namespace ModuleHelpers {

void LoadModulesIf(const std::vector<std::wstring>& paths) {
    for (const auto& path : paths) {
        if (GetModuleHandleW(path.c_str())) {
            Log::Write(Log::Level::Info, L"Module already loaded: %s", path.c_str());
            continue;
        }

        // We're not calling FreeLibrary on purpose.
        HMODULE module = LoadLibraryW(path.c_str());
        if (!module) {
            DWORD error = GetLastError();
            Log::Write(Log::Level::Error, L"LoadLibrary failed: %s (%lu)", path.c_str(), error);
            wchar_t message[1024] = {};
            StringCchPrintfW(message, ARRAYSIZE(message),
                L"LoadLibrary failed.\nPath: %s\nError: %lu", path.c_str(), error);
            MessageBoxW(nullptr, message, L"Explorer Modules", MB_ICONERROR | MB_OK);
            continue;
        }
        Log::Write(Log::Level::Info, L"LoadLibrary succeeded: %s", path.c_str());
    }
}

std::vector<ModuleInfo> GetLoadedModules() {
    std::vector<ModuleInfo> items;

    // Initial guess
    std::vector<HMODULE> modules(1024);
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed)) {
        Log::Write(Log::Level::Error, L"EnumProcessModules failed: %lu", GetLastError());
        return items;
    }

    DWORD count = needed / sizeof(HMODULE);
    if (count > modules.size()) {
        modules.resize(count);
        // Retry with larger buffer
        if (!EnumProcessModules(GetCurrentProcess(), modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed)) {
            Log::Write(Log::Level::Error, L"EnumProcessModules (retry) failed: %lu", GetLastError());
            return items;
        }
    }

    Log::Write(Log::Level::Info, L"Enumerating %lu modules", count);
    for (DWORD i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(modules[i], path, ARRAYSIZE(path))) {
            MODULEINFO info = {};
            if (GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info))) {
                 items.push_back({path, info.lpBaseOfDll, info.SizeOfImage});
                 Log::Write(Log::Level::Trace, L"Module: %s Base=%p Size=%u", path, info.lpBaseOfDll, info.SizeOfImage);
            } else {
                 items.push_back({path, modules[i], 0});
                 Log::Write(Log::Level::Warn, L"GetModuleInformation failed for: %s", path);
            }
        }
    }

    return items;
}

}
