#include "DllNotification.h"
#include "Log.h"
#include "ModuleFolder.h"

#include <windows.h>
#include <winternl.h> // For UNICODE_STRING
#include <shlobj.h>
#include <thread>
#include <string>

// LDR notification definitions
typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;
    PCUNICODE_STRING FullDllName;
    PCUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
    ULONG Flags;
    PCUNICODE_STRING FullDllName;
    PCUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PCLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK *PLDR_DLL_NOTIFICATION_FUNCTION)(
    _In_     ULONG                       NotificationReason,
    _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
    _In_opt_ PVOID                       Context
);

// Reason codes
#ifndef LDR_DLL_NOTIFICATION_REASON_LOADED
#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#endif

#ifndef LDR_DLL_NOTIFICATION_REASON_UNLOADED
#define LDR_DLL_NOTIFICATION_REASON_UNLOADED 2
#endif

typedef NTSTATUS (NTAPI *LdrRegisterDllNotification_t)(
    _In_     ULONG                          Flags,
    _In_     PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    _In_opt_ PVOID                          Context,
    _Out_    PVOID                          *Cookie
);

typedef NTSTATUS (NTAPI *LdrUnregisterDllNotification_t)(
    _In_     PVOID Cookie
);

namespace {
    PVOID g_notificationCookie = nullptr;
    LdrRegisterDllNotification_t g_LdrRegisterDllNotification = nullptr;
    LdrUnregisterDllNotification_t g_LdrUnregisterDllNotification = nullptr;
    
    void RefreshWindowThread() {
        // CoInitialize is required for many Shell APIs, though SHChangeNotify might not strictly require it, 
        // relying on parsing definitely does if it involves COM objects.
        HRESULT hr = CoInitialize(nullptr);
        
        // Prefix with :: to ensure it parses as a CLSID/Namespace location
        std::wstring parsingName = std::wstring(kNamespaceParentParsingName) + L"\\::" + kModuleFolderClsidString;

        // Notify Explorer that the contents of our folder have changed.
        // SHCNE_UPDATEDIR indicates the contents of the folder identified by the PIDL have changed.
        // SHCNF_PARSE_NAME is not standard, so we must parse it to a PIDL first.
        
        PIDLIST_ABSOLUTE pidl = nullptr;
        SFGAOF sfgao = 0;
        HRESULT hrParse = SHParseDisplayName(parsingName.c_str(), nullptr, &pidl, 0, &sfgao);
        
        if (SUCCEEDED(hrParse)) {
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, pidl, nullptr);
            CoTaskMemFree(pidl);
        } else {
             Log::Write(Log::Level::Error, L"SHParseDisplayName failed for %s: 0x%08X", parsingName.c_str(), hrParse);
        }

        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }

    VOID CALLBACK DllNotificationCallback(
        _In_     ULONG                       NotificationReason,
        _In_     PCLDR_DLL_NOTIFICATION_DATA NotificationData,
        _In_opt_ PVOID                       Context
    ) {
        UNREFERENCED_PARAMETER(NotificationData);
        UNREFERENCED_PARAMETER(Context);

        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED || 
            NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED) {
            
            // Execute the refresh on a separate thread to avoid deadlock issues 
            // "It is unsafe for the notification callback to call functions in ANY other module other than itself."
            std::thread t(RefreshWindowThread);
            t.detach();
        }
    }
}

void InitializeDllNotification() {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) {
        Log::Write(Log::Level::Error, L"Could not get handle to ntdll.dll");
        return;
    }

    g_LdrRegisterDllNotification = (LdrRegisterDllNotification_t)GetProcAddress(hNtdll, "LdrRegisterDllNotification");
    g_LdrUnregisterDllNotification = (LdrUnregisterDllNotification_t)GetProcAddress(hNtdll, "LdrUnregisterDllNotification");

    if (g_LdrRegisterDllNotification && g_LdrUnregisterDllNotification) {
        NTSTATUS status = g_LdrRegisterDllNotification(0, DllNotificationCallback, nullptr, &g_notificationCookie);
        if (status != 0) { // STATUS_SUCCESS = 0
             Log::Write(Log::Level::Error, L"Failed to register DLL notification: 0x%x", status);
             g_notificationCookie = nullptr;
        } else {
             Log::Write(Log::Level::Info, L"Registered DLL notification");
        }
    } else {
        Log::Write(Log::Level::Error, L"Could not find LdrRegisterDllNotification or LdrUnregisterDllNotification in ntdll.dll");
    }
}

void ShutdownDllNotification() {
    if (g_notificationCookie && g_LdrUnregisterDllNotification) {
        g_LdrUnregisterDllNotification(g_notificationCookie);
        g_notificationCookie = nullptr;
        Log::Write(Log::Level::Info, L"Unregistered DLL notification");
    }
}
