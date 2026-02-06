#pragma once
#include <vector>
#include <string>
#include <windows.h>

namespace ModuleHelpers {

struct ModuleInfo {
    std::wstring path;
    void* baseAddress;
    DWORD size;
};

int LoadModulesIf(const std::vector<std::wstring>& paths);

bool UnloadLibrary(void* baseAddress);

std::vector<ModuleInfo> GetLoadedModules();

}
