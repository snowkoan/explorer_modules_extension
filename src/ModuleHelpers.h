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

/// @brief Loads modules from the specified paths if they are not already loaded.
/// @param paths A vector of module file paths to load.
/// @return The number of modules successfully loaded.
int LoadModulesIf(const std::vector<std::wstring>& paths);

/// @brief Attempts to unload a module given its base address.
/// @param baseAddress The base address of the module to unload.
/// @return True if the module was successfully unloaded, false otherwise.
bool UnloadLibrary(void* baseAddress);

/// @brief Gets a list of currently loaded modules in the process.
/// @return A vector of ModuleInfo structures representing the loaded modules.
std::vector<ModuleInfo> GetLoadedModules();

}
