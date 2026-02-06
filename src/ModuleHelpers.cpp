#include "ModuleHelpers.h"
#include "Log.h"
#include <windows.h>
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

}
