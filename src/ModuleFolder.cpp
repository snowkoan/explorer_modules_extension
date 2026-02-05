#include "ModuleFolder.h"

#include "EnumIDList.h"
#include "IidNames.h"
#include "Log.h"
#include "Pidl.h"

#include <propkey.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kColumnName = 0;
constexpr UINT kColumnBase = 1;
constexpr UINT kColumnSize = 2;
constexpr UINT kColumnCount = 3;

UINT CF_SHELLIDLIST() {
    static UINT format = RegisterClipboardFormatW(CFSTR_SHELLIDLIST);
    return format;
}

HRESULT MakeStrRet(const wchar_t* value, STRRET* result) {
    if (!result) {
        return E_POINTER;
    }
    wchar_t* dup = nullptr;
    HRESULT hr = SHStrDupW(value, &dup);
    if (FAILED(hr)) {
        return hr;
    }
    result->uType = STRRET_WSTR;
    result->pOleStr = dup;
    return S_OK;
}

struct ModuleItem {
    std::wstring path;
    void* baseAddress;
    DWORD size;
};

std::vector<ModuleItem> GetModuleItems() {
    std::vector<ModuleItem> items;

    HMODULE modules[1024] = {};
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        Log::Write(Log::Level::Error, L"EnumProcessModules failed: %lu", GetLastError());
        return items;
    }

    DWORD count = needed / sizeof(HMODULE);
    if (count > ARRAYSIZE(modules)) {
        count = ARRAYSIZE(modules);
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

std::vector<std::wstring> ExtractDropPaths(IDataObject* dataObject) {
    std::vector<std::wstring> paths;
    if (!dataObject) {
        return paths;
    }

    FORMATETC hdropFormat = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM medium = {};
    if (SUCCEEDED(dataObject->GetData(&hdropFormat, &medium))) {
        HDROP drop = static_cast<HDROP>(medium.hGlobal);
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        Log::Write(Log::Level::Info, L"CF_HDROP items: %u", count);
        for (UINT i = 0; i < count; ++i) {
            wchar_t path[MAX_PATH] = {};
            if (DragQueryFileW(drop, i, path, ARRAYSIZE(path))) {
                paths.emplace_back(path);
            }
        }
        ReleaseStgMedium(&medium);
        return paths;
    }

    FORMATETC shellFormat = { static_cast<CLIPFORMAT>(CF_SHELLIDLIST()), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (SUCCEEDED(dataObject->GetData(&shellFormat, &medium))) {
        auto cida = static_cast<CIDA*>(GlobalLock(medium.hGlobal));
        if (cida) {
            Log::Write(Log::Level::Info, L"CFSTR_SHELLIDLIST items: %u", cida->cidl);
            auto base = reinterpret_cast<const BYTE*>(cida);
            auto parent = reinterpret_cast<PCIDLIST_ABSOLUTE>(base + cida->aoffset[0]);
            for (UINT i = 0; i < cida->cidl; ++i) {
                auto child = reinterpret_cast<PCUIDLIST_RELATIVE>(base + cida->aoffset[i + 1]);
                PIDLIST_ABSOLUTE full = ILCombine(parent, child);
                if (full) {
                    wchar_t path[MAX_PATH] = {};
                    if (SHGetPathFromIDListW(full, path)) {
                        paths.emplace_back(path);
                    }
                    CoTaskMemFree(full);
                }
            }
            GlobalUnlock(medium.hGlobal);
        }
        ReleaseStgMedium(&medium);
    }

    return paths;
}

void LoadLibraryPaths(const std::vector<std::wstring>& paths);

class ItemContextMenu final
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IContextMenu3> {
public:
    explicit ItemContextMenu(std::vector<std::wstring> paths)
        : paths_(std::move(paths)) {}

    IFACEMETHODIMP QueryContextMenu(HMENU menu, UINT index, UINT idCmdFirst, UINT idCmdLast, UINT flags) override {
        if (!menu) {
            return E_INVALIDARG;
        }
        if (flags & CMF_DEFAULTONLY) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
        }
        if (idCmdFirst > idCmdLast) {
            return E_INVALIDARG;
        }
        InsertMenuW(menu, index, MF_BYPOSITION | MF_STRING, idCmdFirst + kCmdLoad, L"Load Module");
        SetMenuDefaultItem(menu, index, TRUE);
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, kCmdCount);
    }

    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO info) override {
        if (!info) {
            return E_POINTER;
        }
        if (HIWORD(info->lpVerb) != 0) {
            const char* verb = reinterpret_cast<const char*>(info->lpVerb);
            if (lstrcmpiA(verb, "open") != 0 && lstrcmpiA(verb, "load") != 0) {
                return E_FAIL;
            }
        } else if (LOWORD(info->lpVerb) != kCmdLoad) {
            return E_FAIL;
        }

        Log::Write(Log::Level::Info, L"ItemContextMenu::InvokeCommand loading %zu module(s)", paths_.size());
        LoadLibraryPaths(paths_);
        return S_OK;
    }

    IFACEMETHODIMP GetCommandString(UINT_PTR idCmd, UINT type, UINT*, LPSTR name, UINT cchMax) override {
        if (idCmd != kCmdLoad) {
            return E_INVALIDARG;
        }
        if (!name || cchMax == 0) {
            return E_POINTER;
        }

        switch (type) {
        case GCS_HELPTEXTA:
            StringCchCopyA(name, cchMax, "Load the selected module(s).");
            break;
        case GCS_HELPTEXTW:
            StringCchCopyW(reinterpret_cast<LPWSTR>(name), cchMax, L"Load the selected module(s).");
            break;
        case GCS_VERBA:
            StringCchCopyA(name, cchMax, "open");
            break;
        case GCS_VERBW:
            StringCchCopyW(reinterpret_cast<LPWSTR>(name), cchMax, L"open");
            break;
        default:
            break;
        }
        return S_OK;
    }

    IFACEMETHODIMP HandleMenuMsg(UINT, WPARAM, LPARAM) override {
        return S_OK;
    }

    IFACEMETHODIMP HandleMenuMsg2(UINT, WPARAM, LPARAM, LRESULT* result) override {
        if (result) {
            *result = 0;
        }
        return S_OK;
    }

private:
    enum : UINT {
        kCmdLoad = 0,
        kCmdCount = 1
    };

    std::vector<std::wstring> paths_;
};

void LoadLibraryPaths(const std::vector<std::wstring>& paths) {
    for (const auto& path : paths) {
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
        FreeLibrary(module);
    }
}

bool SupportsDropFormat(IDataObject* dataObject) {
    if (!dataObject) {
        return false;
    }
    FORMATETC hdropFormat = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    if (SUCCEEDED(dataObject->QueryGetData(&hdropFormat))) {
        return true;
    }
    FORMATETC shellFormat = { static_cast<CLIPFORMAT>(CF_SHELLIDLIST()), nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    return SUCCEEDED(dataObject->QueryGetData(&shellFormat));
}
} // namespace

ModuleFolder::ModuleFolder() {
    rootPidl_ = nullptr;
    Log::Write(Log::Level::Info, L"ModuleFolder constructed");
}

IFACEMETHODIMP ModuleFolder::GetClassID(CLSID* classId) {
    if (!classId) {
        return E_POINTER;
    }
    *classId = CLSID_ModuleFolder;
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::Initialize(PCIDLIST_ABSOLUTE pidl) {
    Log::Write(Log::Level::Info, L"ModuleFolder::Initialize called (pidl=%p)", pidl);
    if (rootPidl_) {
        ILFree(rootPidl_);
    }
    if (!pidl) {
        Log::Write(Log::Level::Warn, L"ModuleFolder::Initialize received null pidl");
        rootPidl_ = nullptr;
        return S_OK;
    }
    rootPidl_ = ILCloneFull(pidl);
    if (!rootPidl_) {
        Log::Write(Log::Level::Error, L"ModuleFolder::Initialize ILCloneFull failed");
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::GetCurFolder(PIDLIST_ABSOLUTE* pidl) {
    if (!pidl) {
        return E_POINTER;
    }
    if (!rootPidl_) {
        Log::Write(Log::Level::Warn, L"GetCurFolder called without root PIDL");
        *pidl = nullptr;
        return E_FAIL;
    }
    *pidl = ILCloneFull(rootPidl_);
    if (!*pidl) {
        Log::Write(Log::Level::Error, L"GetCurFolder: out of memory");
    }
    return *pidl ? S_OK : E_OUTOFMEMORY;
}

IFACEMETHODIMP ModuleFolder::ParseDisplayName(HWND, IBindCtx*, LPWSTR, ULONG*, PIDLIST_RELATIVE*, ULONG*) {
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::EnumObjects(HWND, SHCONTF flags, IEnumIDList** enumIdList) {
    Log::Write(Log::Level::Info, L"EnumObjects called (flags=0x%X)", flags);
    if (!enumIdList) {
        return E_POINTER;
    }
    *enumIdList = nullptr;
    if (!(flags & SHCONTF_NONFOLDERS)) {
        Log::Write(Log::Level::Info, L"EnumObjects skipped (NONFOLDERS not requested)");
        return S_FALSE;
    }

    std::vector<PIDLIST_RELATIVE> items;
    auto moduleItems = GetModuleItems();
    items.reserve(moduleItems.size());
    for (const auto& item : moduleItems) {
        auto pidl = Pidl::CreateFromPath(item.path, item.baseAddress, item.size);
        if (pidl) {
            items.push_back(pidl);
        }
    }

    auto enumerator = Microsoft::WRL::Make<EnumIDList>(items);
    for (auto item : items) {
        Pidl::Free(item);
    }

    if (!enumerator) {
        Log::Write(Log::Level::Error, L"EnumIDList allocation failed");
        return E_OUTOFMEMORY;
    }
    if (items.empty()) {
        Log::Write(Log::Level::Info, L"EnumObjects returning 0 items");
        return S_FALSE;
    }
    Log::Write(Log::Level::Info, L"EnumObjects returning %zu items", items.size());
    return enumerator.CopyTo(enumIdList);
}

IFACEMETHODIMP ModuleFolder::BindToObject(PCUIDLIST_RELATIVE, IBindCtx*, REFIID, void** ppv) {
    if (ppv) {
        *ppv = nullptr;
    }
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::BindToStorage(PCUIDLIST_RELATIVE, IBindCtx*, REFIID, void** ppv) {
    if (ppv) {
        *ppv = nullptr;
    }
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::CompareIDs(LPARAM, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) {
    if (!pidl1 || !pidl2) {
        return E_INVALIDARG;
    }
    
    // Check if PIDLs are ours
    bool ours1 = Pidl::IsOurPidl(pidl1);
    bool ours2 = Pidl::IsOurPidl(pidl2);

    // If both are ours, compare paths
    if (ours1 && ours2) {
        auto path1 = Pidl::GetPath(pidl1);
        auto path2 = Pidl::GetPath(pidl2);
        int result = lstrcmpiW(path1.c_str(), path2.c_str());
        short compare = static_cast<short>(result);
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(compare));
    }

    // If one is ours and one is not, they are different
    if (ours1 != ours2) {
        // Deterministic ordering: ours first (or last, doesn't matter much)
        short compare = ours1 ? -1 : 1;
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(compare));
    }

    // Neither is ours - rely on binary comparison or defer
    // Since we don't know structure, we can only do binary compare of the item ID
    // Note: This relies on cb being first USHORT
    if (pidl1->mkid.cb != pidl2->mkid.cb) {
        short compare = (pidl1->mkid.cb < pidl2->mkid.cb) ? -1 : 1;
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(compare));
    }
    int result = memcmp(&pidl1->mkid.abID, &pidl2->mkid.abID, pidl1->mkid.cb - sizeof(USHORT));
    short compare = static_cast<short>(result);
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, static_cast<USHORT>(compare));
}

IFACEMETHODIMP ModuleFolder::CreateViewObject(HWND, REFIID riid, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IShellView)) {
        Log::Write(Log::Level::Info, L"CreateViewObject called");
        SFV_CREATE sfv = {};
        sfv.cbSize = sizeof(sfv);
        ComPtr<IShellFolder> shellFolder;
        HRESULT hr = QueryInterface(IID_PPV_ARGS(&shellFolder));
        if (FAILED(hr)) {
            Log::Write(Log::Level::Error, L"CreateViewObject QI IShellFolder failed: 0x%08X", hr);
            return hr;
        }
        sfv.pshf = shellFolder.Get();
            sfv.psfvcb = static_cast<IShellFolderViewCB*>(this);
        hr = SHCreateShellFolderView(&sfv, reinterpret_cast<IShellView**>(ppv));
        if (!SUCCEEDED(hr)) {
            Log::Write(Log::Level::Info, L"SHCreateShellFolderView hr=0x%08X", hr);
        }
        return hr;
    }
    if (IsEqualIID(riid, IID_IDropTarget)) {
        Log::Write(Log::Level::Info, L"CreateViewObject returning IDropTarget");
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP ModuleFolder::GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) {
    if (!rgfInOut) {
        return E_POINTER;
    }
    SFGAOF folderAttrs = SFGAO_FOLDER | SFGAO_HASSUBFOLDER | SFGAO_BROWSABLE;
    SFGAOF itemAttrs = SFGAO_STREAM | SFGAO_READONLY;
    SFGAOF attrs = (cidl == 0 || !apidl) ? folderAttrs : itemAttrs;
    if (*rgfInOut) {
        *rgfInOut &= attrs;
    } else {
        *rgfInOut = attrs;
    }
    Log::Write(Log::Level::Trace, L"GetAttributesOf %s attrs=0x%08X",
        (cidl == 0 || !apidl) ? L"folder" : L"item", *rgfInOut);
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::GetUIObjectOf(HWND, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
    REFIID riid, UINT*, void** ppv) {
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    Log::Write(Log::Level::Info, L"GetUIObjectOf cidl=%u riid=%s", cidl, IidNames::ToString(riid).c_str());
    if (cidl == 0 && IsEqualIID(riid, IID_IDropTarget)) {
        *ppv = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    if (cidl > 0 && (IsEqualIID(riid, IID_IContextMenu) || IsEqualIID(riid, IID_IContextMenu2) || IsEqualIID(riid, IID_IContextMenu3))) {
        std::vector<std::wstring> paths;
        paths.reserve(cidl);
        for (UINT i = 0; i < cidl; ++i) {
            if (!Pidl::IsOurPidl(apidl[i])) {
                continue;
            }
            auto path = Pidl::GetPath(apidl[i]);
            if (!path.empty()) {
                paths.push_back(std::move(path));
            }
        }
        if (paths.empty()) {
            return E_FAIL;
        }
        auto menu = Microsoft::WRL::Make<ItemContextMenu>(std::move(paths));
        if (!menu) {
            return E_OUTOFMEMORY;
        }
        return menu.CopyTo(reinterpret_cast<IContextMenu3**>(ppv));
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP ModuleFolder::GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF, STRRET* name) {
    if (!pidl || !name) {
        return E_INVALIDARG;
    }
    if (!Pidl::IsOurPidl(pidl)) {
        return E_INVALIDARG;
    }
    auto path = Pidl::GetPath(pidl);
    const wchar_t* base = PathFindFileNameW(path.c_str());
    return MakeStrRet(base, name);
}

IFACEMETHODIMP ModuleFolder::SetNameOf(HWND, PCUITEMID_CHILD, LPCWSTR, SHGDNF, PITEMID_CHILD* newPidl) {
    if (newPidl) {
        *newPidl = nullptr;
    }
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::GetDefaultSearchGUID(GUID*) {
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::EnumSearches(IEnumExtraSearch**) {
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::GetDefaultColumn(DWORD, ULONG* sort, ULONG* display) {
    if (sort) {
        *sort = kColumnName;
    }
    if (display) {
        *display = kColumnName;
    }
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::GetDefaultColumnState(UINT, SHCOLSTATEF* state) {
    if (!state) {
        return E_POINTER;
    }
    *state = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT;
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::GetDetailsEx(PCUITEMID_CHILD, const SHCOLUMNID*, VARIANT*) {
    return E_NOTIMPL;
}

IFACEMETHODIMP ModuleFolder::GetDetailsOf(PCUITEMID_CHILD pidl, UINT column, SHELLDETAILS* details) {
    if (!details) {
        return E_POINTER;
    }
    details->fmt = LVCFMT_LEFT;
    details->cxChar = 24;

    if (!pidl) {
        switch (column) {
        case kColumnName:
            return MakeStrRet(L"Name", &details->str);
        case kColumnBase:
            return MakeStrRet(L"Base Address", &details->str);
        case kColumnSize:
            return MakeStrRet(L"Size", &details->str);
        default:
            return E_INVALIDARG;
        }
    }

    if (!Pidl::IsOurPidl(pidl)) {
        Log::Write(Log::Level::Warn, L"GetDetailsOf: Invalid PIDL for column %u", column);
        return E_INVALIDARG;
    }

    auto path = Pidl::GetPath(pidl);
    // Log::Write(Log::Level::Trace, L"GetDetailsOf: col=%u path=%s", column, path.c_str());

    switch (column) {
    case kColumnName: {
        const wchar_t* base = PathFindFileNameW(path.c_str());
        return MakeStrRet(base, &details->str);
    }
    case kColumnBase: {
        void* addr = Pidl::GetBaseAddress(pidl);
        wchar_t text[64] = {};
        StringCchPrintfW(text, ARRAYSIZE(text), L"0x%p", addr);
        Log::Write(Log::Level::Trace, L"GetDetailsOf: BaseAddress=%s for %s", text, path.c_str());
        return MakeStrRet(text, &details->str);
    }
    case kColumnSize: {
        DWORD size = Pidl::GetSize(pidl);
        wchar_t text[64] = {};
        StringCchPrintfW(text, ARRAYSIZE(text), L"0x%X (%u)", size, size);
        Log::Write(Log::Level::Trace, L"GetDetailsOf: Size=%s for %s", text, path.c_str());
        return MakeStrRet(text, &details->str);
    }
    default:
        return E_INVALIDARG;
    }
}

IFACEMETHODIMP ModuleFolder::MapColumnToSCID(UINT column, SHCOLUMNID* pscid) {
    UNREFERENCED_PARAMETER(column);
    UNREFERENCED_PARAMETER(pscid);
    // If we return E_NOTIMPL, Explorer falls back to GetDetailsOf, which is exactly what we want
    // unless we implement GetDetailsEx generally.
    // For now, disabling this mapping fixes the "empty columns" issue because GetDetailsEx is E_NOTIMPL.
    return E_NOTIMPL; 
    
    /* 
    if (!pscid) {
        return E_POINTER;
    }
    switch (column) {
    case kColumnName:
        *pscid = PKEY_ItemNameDisplay;
        return S_OK;
    case kColumnBase:
        *pscid = PKEY_ItemTypeText; // This was probably wrong anyway
        return S_OK;
    case kColumnSize:
        *pscid = PKEY_Size;
        return S_OK;
    default:
        return E_INVALIDARG;
    }
    */
}

IFACEMETHODIMP ModuleFolder::DragEnter(IDataObject* dataObject, DWORD, POINTL, DWORD* effect) {
    if (!effect) {
        return E_POINTER;
    }
    if (SupportsDropFormat(dataObject)) {
        *effect = DROPEFFECT_COPY;
        Log::Write(Log::Level::Info, L"DragEnter: accepted");
    } else {
        *effect = DROPEFFECT_NONE;
        Log::Write(Log::Level::Info, L"DragEnter: rejected");
    }
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::DragOver(DWORD, POINTL, DWORD* effect) {
    if (!effect) {
        return E_POINTER;
    }
    *effect = DROPEFFECT_COPY;
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::DragLeave() {
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::Drop(IDataObject* dataObject, DWORD, POINTL, DWORD* effect) {
    if (effect) {
        *effect = DROPEFFECT_COPY;
    }
    auto paths = ExtractDropPaths(dataObject);
    Log::Write(Log::Level::Info, L"Drop received %zu paths", paths.size());
    if (!paths.empty()) {
        LoadLibraryPaths(paths);
    }
    return S_OK;
}

IFACEMETHODIMP ModuleFolder::MessageSFVCB(UINT msg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);
    
    if (msg == SFVM_GETNOTIFY) {
        Log::Write(Log::Level::Trace, L"MessageSFVCB msg=SFVM_GETNOTIFY");
    }
    
    // Return E_NOTIMPL to let the default view handler process messages
    return E_NOTIMPL;
}
