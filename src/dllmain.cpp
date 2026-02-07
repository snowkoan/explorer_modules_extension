#include "ModuleFolder.h"
#include "Log.h"
#include "DllNotification.h"

#include <shlobj.h>
#include <strsafe.h>
#include <wrl/module.h>
#include "IidNames.h"

extern HRESULT CreateModuleFolderClassFactory(REFIID riid, void** ppv);

HMODULE g_module = nullptr;

namespace {

// Registry path constants
constexpr wchar_t kKeyClsidRoot[] = L"Software\\Classes\\CLSID";
constexpr wchar_t kKeyNamespaceRoot[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace";

// RAII wrapper for HKEY handles to ensure resources are properly released.
class ScopedKey {
public:
    ScopedKey() : m_key(nullptr) {}
    ~ScopedKey() { Close(); }

    // Non-copyable to prevent double-free
    ScopedKey(const ScopedKey&) = delete;
    ScopedKey& operator=(const ScopedKey&) = delete;

    // Allow taking address for API calls that output an HKEY (e.g., RegCreateKeyEx)
    HKEY* operator&() {
        Close();
        return &m_key;
    }

    // Implicit conversion to HKEY for API calls that take an input HKEY
    operator HKEY() const {
        return m_key;
    }

    void Close() {
        if (m_key) {
            RegCloseKey(m_key);
            m_key = nullptr;
        }
    }

private:
    HKEY m_key;
};

HRESULT SetStringValue(HKEY key, const wchar_t* name, const wchar_t* value) {
    return HRESULT_FROM_WIN32(RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))));
}

HRESULT SetDwordValue(HKEY key, const wchar_t* name, DWORD value) {
    return HRESULT_FROM_WIN32(RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        static_cast<DWORD>(sizeof(value))));
}

HRESULT RegisterClsid(const wchar_t* modulePath) {
    ScopedKey clsidKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kKeyClsidRoot,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &clsidKey,
        nullptr));
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"RegCreateKeyExW CLSID failed: 0x%08X", hr);
        return hr;
    }

    ScopedKey classKey;
    hr = HRESULT_FROM_WIN32(RegCreateKeyExW(
        clsidKey,
        kModuleFolderClsidString,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &classKey,
        nullptr));
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"RegCreateKeyExW CLSID subkey failed: 0x%08X", hr);
        return hr;
    }

    hr = SetStringValue(classKey, nullptr, kModuleFolderDisplayName);
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"Set CLSID display name failed: 0x%08X", hr);
        return hr;
    }

    SetDwordValue(classKey, L"System.IsPinnedToNameSpaceTree", 1);

    {
        // "Implemented Categories" is just a marker key, no values needed.
        ScopedKey categoryKey;
        RegCreateKeyExW(
            classKey,
            L"Implemented Categories\\{00021490-0000-0000-C000-000000000046}",
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &categoryKey,
            nullptr);
    }

    ScopedKey inprocKey;
    hr = HRESULT_FROM_WIN32(RegCreateKeyExW(
        classKey,
        L"InProcServer32",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &inprocKey,
        nullptr));
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"RegCreateKeyExW InProcServer32 failed: 0x%08X", hr);
        return hr;
    }

    hr = SetStringValue(inprocKey, nullptr, modulePath);
    if (SUCCEEDED(hr)) {
        hr = SetStringValue(inprocKey, L"ThreadingModel", L"Apartment");
    }
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"Set InProcServer32 values failed: 0x%08X", hr);
        return hr;
    }

    ScopedKey shellFolderKey;
    if (RegCreateKeyExW(classKey, L"ShellFolder", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &shellFolderKey, nullptr) == ERROR_SUCCESS) {
        SetDwordValue(shellFolderKey, L"Attributes", SFGAO_FOLDER | SFGAO_DROPTARGET);
        SetDwordValue(shellFolderKey, L"FolderValueFlags", FWF_NOSUBFOLDERS);
    }

    ScopedKey iconKey;
    if (RegCreateKeyExW(classKey, L"DefaultIcon", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &iconKey, nullptr) == ERROR_SUCCESS) {
        wchar_t iconValue[MAX_PATH * 2] = {};
        StringCchPrintfW(iconValue, ARRAYSIZE(iconValue), L"%s,-1", modulePath);
        SetStringValue(iconKey, nullptr, iconValue);
    }

    Log::Write(Log::Level::Info, L"Registered CLSID %s", kModuleFolderClsidString);
    return S_OK;
}

HRESULT RegisterNamespace() {
    ScopedKey nsKey;
    HRESULT hr = HRESULT_FROM_WIN32(RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kKeyNamespaceRoot,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &nsKey,
        nullptr));
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"RegCreateKeyExW MyComputer NameSpace failed: 0x%08X", hr);
        return hr;
    }

    ScopedKey clsidKey;
    hr = HRESULT_FROM_WIN32(RegCreateKeyExW(
        nsKey,
        kModuleFolderClsidString,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        nullptr,
        &clsidKey,
        nullptr));
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"RegCreateKeyExW NameSpace CLSID failed: 0x%08X", hr);
        return hr;
    }

    hr = SetStringValue(clsidKey, nullptr, kModuleFolderDisplayName);
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"Set NameSpace display name failed: 0x%08X", hr);
    } else {
        Log::Write(Log::Level::Info, L"Registered NameSpace entry for %s", kModuleFolderClsidString);
    }
    return hr;
}

HRESULT UnregisterTree(HKEY root, const wchar_t* subkey) {
    return HRESULT_FROM_WIN32(RegDeleteTreeW(root, subkey));
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Create();
        DisableThreadLibraryCalls(module);
        InitializeDllNotification();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        ShutdownDllNotification();
        Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule().Terminate();
    }
    return TRUE;
}

extern "C" STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {

    Log::Write(Log::Level::Trace, L"DllGetClassObject called: clsid=%s, riid=%s", 
        IidNames::ToString(clsid).c_str(), IidNames::ToString(riid).c_str());
    
    if (!IsEqualCLSID(clsid, CLSID_ModuleFolder)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    return CreateModuleFolderClassFactory(riid, ppv);
}

extern "C" STDAPI DllCanUnloadNow() {
    auto& module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
    return (module.GetObjectCount() == 0) ? S_OK : S_FALSE;
}

extern "C" STDAPI DllRegisterServer() {
    wchar_t modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_module, modulePath, ARRAYSIZE(modulePath))) {
        Log::Write(Log::Level::Error, L"GetModuleFileNameW failed: %lu", GetLastError());
        return HRESULT_FROM_WIN32(GetLastError());
    }

    Log::Write(Log::Level::Info, L"DllRegisterServer: %s", modulePath);
    HRESULT hr = RegisterClsid(modulePath);
    if (SUCCEEDED(hr)) {
        hr = RegisterNamespace();
    }
    if (FAILED(hr)) {
        Log::Write(Log::Level::Error, L"DllRegisterServer failed: 0x%08X", hr);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return hr;
}

extern "C" STDAPI DllUnregisterServer() {
    wchar_t clsidPath[MAX_PATH] = {};
    StringCchPrintfW(clsidPath, ARRAYSIZE(clsidPath),
        L"%s\\%s", kKeyClsidRoot, kModuleFolderClsidString);
    UnregisterTree(HKEY_CURRENT_USER, clsidPath);

    wchar_t nsPath[MAX_PATH] = {};
    StringCchPrintfW(nsPath, ARRAYSIZE(nsPath),
        L"%s\\%s", kKeyNamespaceRoot, kModuleFolderClsidString);
    UnregisterTree(HKEY_CURRENT_USER, nsPath);

    Log::Write(Log::Level::Info, L"DllUnregisterServer completed");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
