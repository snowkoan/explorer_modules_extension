#pragma once

#include <windows.h>
#include <shlobj.h>
#include <wrl.h>

// {6B4E2E3B-3D6B-4D4E-9A1C-0F0C8D8E8F11}
static const CLSID CLSID_ModuleFolder =
{ 0x6b4e2e3b, 0x3d6b, 0x4d4e, { 0x9a, 0x1c, 0x0f, 0x0c, 0x8d, 0x8e, 0x8f, 0x11 } };

static constexpr wchar_t kModuleFolderClsidString[] = L"{6B4E2E3B-3D6B-4D4E-9A1C-0F0C8D8E8F11}";
static constexpr wchar_t kModuleFolderDisplayName[] = L"Explorer Modules";
static constexpr wchar_t kModuleFolderClassName[] = L"ExplorerModulesNamespace.ModuleFolder";

class ModuleFolder final
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IShellFolder, // Communication channel between Explorer and the namespace extension
        IShellFolder2, // Extended IShellFolder with more features
        IPersistFolder, // Required for namespace extension initialization
        IPersistFolder2,
        IShellFolderViewCB,
        IDropTarget> {
public:
    ModuleFolder();

    // IPersist
    IFACEMETHODIMP GetClassID(CLSID* classId) override;

    // IPersistFolder
    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidl) override;

    // IPersistFolder2
    IFACEMETHODIMP GetCurFolder(PIDLIST_ABSOLUTE* pidl) override;

    // IShellFolder
    IFACEMETHODIMP ParseDisplayName(HWND hwnd, IBindCtx* bindCtx, LPWSTR displayName,
        ULONG* eaten, PIDLIST_RELATIVE* pidl, ULONG* attributes) override;
    IFACEMETHODIMP EnumObjects(HWND hwnd, SHCONTF flags, IEnumIDList** enumIdList) override;
    IFACEMETHODIMP BindToObject(PCUIDLIST_RELATIVE pidl, IBindCtx* bindCtx, REFIID riid, void** ppv) override;
    IFACEMETHODIMP BindToStorage(PCUIDLIST_RELATIVE pidl, IBindCtx* bindCtx, REFIID riid, void** ppv) override;
    IFACEMETHODIMP CompareIDs(LPARAM lParam, PCUIDLIST_RELATIVE pidl1, PCUIDLIST_RELATIVE pidl2) override;
    IFACEMETHODIMP CreateViewObject(HWND hwnd, REFIID riid, void** ppv) override;
    IFACEMETHODIMP GetAttributesOf(UINT cidl, PCUITEMID_CHILD_ARRAY apidl, SFGAOF* rgfInOut) override;
    IFACEMETHODIMP GetUIObjectOf(HWND hwnd, UINT cidl, PCUITEMID_CHILD_ARRAY apidl,
        REFIID riid, UINT* rgfReserved, void** ppv) override;
    IFACEMETHODIMP GetDisplayNameOf(PCUITEMID_CHILD pidl, SHGDNF flags, STRRET* name) override;
    IFACEMETHODIMP SetNameOf(HWND hwnd, PCUITEMID_CHILD pidl, LPCWSTR name, SHGDNF flags,
        PITEMID_CHILD* newPidl) override;

    // IShellFolder2
    IFACEMETHODIMP GetDefaultSearchGUID(GUID* pguid) override;
    IFACEMETHODIMP EnumSearches(IEnumExtraSearch** ppenum) override;
    IFACEMETHODIMP GetDefaultColumn(DWORD reserved, ULONG* sort, ULONG* display) override;
    IFACEMETHODIMP GetDefaultColumnState(UINT column, SHCOLSTATEF* state) override;
    IFACEMETHODIMP GetDetailsEx(PCUITEMID_CHILD pidl, const SHCOLUMNID* pscid, VARIANT* pv) override;
    IFACEMETHODIMP GetDetailsOf(PCUITEMID_CHILD pidl, UINT column, SHELLDETAILS* details) override;
    IFACEMETHODIMP MapColumnToSCID(UINT column, SHCOLUMNID* pscid) override;

    // IDropTarget
    IFACEMETHODIMP DragEnter(IDataObject* dataObject, DWORD keyState, POINTL pt, DWORD* effect) override;
    IFACEMETHODIMP DragOver(DWORD keyState, POINTL pt, DWORD* effect) override;
    IFACEMETHODIMP DragLeave() override;
    IFACEMETHODIMP Drop(IDataObject* dataObject, DWORD keyState, POINTL pt, DWORD* effect) override;

    // IShellFolderViewCB
    IFACEMETHODIMP MessageSFVCB(UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    PIDLIST_ABSOLUTE rootPidl_ = nullptr;
    bool canDrop_ = false;
};
