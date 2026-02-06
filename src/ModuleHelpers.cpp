#include "ModuleHelpers.h"
#include "Log.h"
#include <windows.h>
#include <psapi.h>
#include <strsafe.h>

namespace ModuleHelpers {

ImageInfo GetImageInfo(const std::wstring& path) {
    ImageInfo info;
    info.machineType = L"Unknown";

    // Version Info
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size > 0) {
        std::vector<BYTE> buffer(size);
        if (GetFileVersionInfoW(path.c_str(), handle, size, buffer.data())) {
            struct LANGANDCODEPAGE {
                WORD wLanguage;
                WORD wCodePage;
            } *lpTranslate;
            UINT cbTranslate = 0;

            if (VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate)) {
                 for(unsigned int i=0; i < (cbTranslate/sizeof(struct LANGANDCODEPAGE)); i++ ) {
                     wchar_t subBlock[256];
                     // CompanyName
                     StringCchPrintfW(subBlock, ARRAYSIZE(subBlock),
                        L"\\StringFileInfo\\%04x%04x\\CompanyName",
                        lpTranslate[i].wLanguage,
                        lpTranslate[i].wCodePage);
                     void* bufferValue = nullptr;
                     UINT len = 0;
                     if (VerQueryValueW(buffer.data(), subBlock, &bufferValue, &len)) {
                         info.companyName = static_cast<wchar_t*>(bufferValue);
                     }
                     
                     // FileVersion
                     StringCchPrintfW(subBlock, ARRAYSIZE(subBlock),
                        L"\\StringFileInfo\\%04x%04x\\FileVersion",
                        lpTranslate[i].wLanguage,
                        lpTranslate[i].wCodePage);
                     if (VerQueryValueW(buffer.data(), subBlock, &bufferValue, &len)) {
                         info.fileVersion = static_cast<wchar_t*>(bufferValue);
                     }
                     
                     if (!info.companyName.empty() || !info.fileVersion.empty()) break; 
                 }
            }
        }
    }

    // Machine Type - Read PE Header
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (hMap) {
            void* pBase = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
            if (pBase) {
                PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pBase;
                if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
                    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)pBase + dosHeader->e_lfanew);
                    if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
                        switch (ntHeaders->FileHeader.Machine) {
                        case IMAGE_FILE_MACHINE_I386: info.machineType = L"x86"; break;
                        case IMAGE_FILE_MACHINE_AMD64: info.machineType = L"x64"; break;
                        case IMAGE_FILE_MACHINE_ARM: info.machineType = L"ARM"; break;
                        case IMAGE_FILE_MACHINE_ARM64: info.machineType = L"ARM64"; break;
                        default: {
                            wchar_t buf[32];
                            StringCchPrintfW(buf, ARRAYSIZE(buf), L"0x%04X", ntHeaders->FileHeader.Machine);
                            info.machineType = buf;
                            break;
                        }
                        }
                    }
                }
                UnmapViewOfFile(pBase);
            }
            CloseHandle(hMap);
        }
        CloseHandle(hFile);
    }

    return info;
}

int LoadModulesIf(const std::vector<std::wstring>& paths) {
    int loadedCount = 0;
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
        loadedCount++;
    }
    return loadedCount;
}

bool UnloadLibrary(void* baseAddress) {
    if (!baseAddress) return false;
    
    bool success = false;
    bool unloaded = false;
    HMODULE hModule = static_cast<HMODULE>(baseAddress);

    // Call FreeLibrary repeatedly to decrement reference count until it hits zero,
    // or until we hit a safety limit.
    int i = 0;
    for (i = 0; i < 100; ++i) {
        if (FreeLibrary(hModule)) {
            success = true;
        } else {
            unloaded = true;
            break;
        }
    }

    if (!success) {
        Log::Write(Log::Level::Error, L"FreeLibrary failed: %lu", GetLastError());
    }
    else if (unloaded) {
        Log::Write(Log::Level::Info, L"Module unloaded after %d attempts", i);
    }
    else
    {
        Log::Write(Log::Level::Warn, L"Module is still loaded after %d FreeLibrary attempts", i);
        success = false;
    }
    
    return success;
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
