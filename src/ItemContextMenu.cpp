#include "ItemContextMenu.h"
#include "Log.h"
#include "ModuleHelpers.h"

#include <shlwapi.h>
#include <strsafe.h>

ItemContextMenu::ItemContextMenu(std::vector<ContextMenuItemData> items, PIDLIST_ABSOLUTE folderPidl)
    : items_(std::move(items)) {
    folderPidl_ = folderPidl ? ILCloneFull(folderPidl) : nullptr;
}

ItemContextMenu::~ItemContextMenu() {
    if (folderPidl_) {
        ILFree(folderPidl_);
    }
}

IFACEMETHODIMP ItemContextMenu::QueryContextMenu(HMENU menu, UINT index, UINT idCmdFirst, UINT idCmdLast, UINT flags) {
    if (!menu) {
        return E_INVALIDARG;
    }
    if (flags & CMF_DEFAULTONLY) {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    }
    
    UINT id = idCmdFirst;
    if (id + kCmdCount - 1 > idCmdLast) {
        return E_FAIL; // Not enough IDs
    }

    InsertMenuW(menu, index++, MF_BYPOSITION | MF_STRING, id + kCmdExplore, L"Explore to parent folder");
    InsertMenuW(menu, index++, MF_BYPOSITION | MF_STRING, id + kCmdProperties, L"Properties");
    InsertMenuW(menu, index++, MF_BYPOSITION | MF_STRING, id + kCmdUnload, L"Unload");
    InsertMenuW(menu, index++, MF_BYPOSITION | MF_STRING, id + kCmdCopyPath, L"Copy path");
    
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, kCmdCount);
}

IFACEMETHODIMP ItemContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO info) {
    if (!info) {
        return E_POINTER;
    }
    
    UINT cmd = kCmdCount;

    if (HIWORD(info->lpVerb) != 0) {
        const char* verb = reinterpret_cast<const char*>(info->lpVerb);
        if (lstrcmpiA(verb, "explore") == 0) {
            cmd = kCmdExplore;
        } else if (lstrcmpiA(verb, "properties") == 0) {
            cmd = kCmdProperties;
        } else if (lstrcmpiA(verb, "unload") == 0) {
            cmd = kCmdUnload;
        } else if (lstrcmpiA(verb, "copypath") == 0) {
            cmd = kCmdCopyPath;
        } else {
            return E_FAIL;
        }
    } else {
        cmd = LOWORD(info->lpVerb);
    }

    switch (cmd) {
    case kCmdExplore:
        for (const auto& item : items_) {
            PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(item.path.c_str());
            if (pidl) {
                SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
                ILFree(pidl);
            }
        }
        break;
    case kCmdProperties:
        for (const auto& item : items_) {
            SHObjectProperties(nullptr, SHOP_FILEPATH, item.path.c_str(), nullptr);
        }
        break;
    case kCmdUnload: {
        bool refresh = false;
        for (const auto& item : items_) {
            if (item.baseAddress) {
                Log::Write(Log::Level::Info, L"Unloading module at %p: %s", item.baseAddress, item.path.c_str());
                if (ModuleHelpers::UnloadLibrary(item.baseAddress)) {
                        refresh = true;
                }
            }
        }
        if (refresh && folderPidl_) {
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, folderPidl_, nullptr);
        }
        break;
    }
    case kCmdCopyPath: {
        if (OpenClipboard(info->hwnd)) {
            EmptyClipboard();
            std::wstring allPaths;
            for (const auto& item : items_) {
                if (!allPaths.empty()) allPaths += L"\r\n";
                allPaths += item.path;
            }
            size_t size = (allPaths.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) {
                void* pMem = GlobalLock(hMem);
                if (pMem) {
                    memcpy(pMem, allPaths.c_str(), size);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                } else {
                    GlobalFree(hMem);
                }
            }
            CloseClipboard();
        }
        break;
    }
    default:
        return E_FAIL;
    }
    return S_OK;
}

IFACEMETHODIMP ItemContextMenu::GetCommandString(UINT_PTR idCmd, UINT type, UINT*, LPSTR name, UINT cchMax) {
    if (!name || cchMax == 0) {
        return E_POINTER;
    }

    switch (idCmd) {
    case kCmdExplore:
        return HandleString(type, name, cchMax, "explore", L"explore", "Open parent folder.", L"Open parent folder.");
    case kCmdProperties:
        return HandleString(type, name, cchMax, "properties", L"properties", "Show properties.", L"Show properties.");
    case kCmdUnload:
        return HandleString(type, name, cchMax, "unload", L"unload", "Unload the module.", L"Unload the module.");
    case kCmdCopyPath:
        return HandleString(type, name, cchMax, "copypath", L"copypath", "Copy module path.", L"Copy module path.");
    default:
        return E_INVALIDARG;
    }
}

IFACEMETHODIMP ItemContextMenu::HandleMenuMsg(UINT, WPARAM, LPARAM) {
    return S_OK;
}

IFACEMETHODIMP ItemContextMenu::HandleMenuMsg2(UINT, WPARAM, LPARAM, LRESULT* result) {
    if (result) {
        *result = 0;
    }
    return S_OK;
}

HRESULT ItemContextMenu::HandleString(UINT type, LPSTR name, UINT cchMax, const char* verbA, const wchar_t* verbW, const char* helpA, const wchar_t* helpW) {
    switch (type) {
    case GCS_HELPTEXTA:
        return StringCchCopyA(name, cchMax, helpA);
    case GCS_HELPTEXTW:
        return StringCchCopyW(reinterpret_cast<LPWSTR>(name), cchMax, helpW);
    case GCS_VERBA:
        return StringCchCopyA(name, cchMax, verbA);
    case GCS_VERBW:
        return StringCchCopyW(reinterpret_cast<LPWSTR>(name), cchMax, verbW);
    }
    return E_NOTIMPL;
}
