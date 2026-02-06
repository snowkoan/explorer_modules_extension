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

void LoadModulesIf(const std::vector<std::wstring>& paths);

std::vector<ModuleInfo> GetLoadedModules();

}
