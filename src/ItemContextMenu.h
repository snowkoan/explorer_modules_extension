#pragma once

#include <windows.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <wrl.h>

struct ContextMenuItemData {
    std::wstring path;
    void* baseAddress;
};

class ItemContextMenu final
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IContextMenu3, IContextMenu2, IContextMenu> {
public:
    explicit ItemContextMenu(std::vector<ContextMenuItemData> items, PIDLIST_ABSOLUTE folderPidl);
    ~ItemContextMenu();

    IFACEMETHODIMP QueryContextMenu(HMENU menu, UINT index, UINT idCmdFirst, UINT idCmdLast, UINT flags) override;
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO info) override;
    IFACEMETHODIMP GetCommandString(UINT_PTR idCmd, UINT type, UINT*, LPSTR name, UINT cchMax) override;
    IFACEMETHODIMP HandleMenuMsg(UINT, WPARAM, LPARAM) override;
    IFACEMETHODIMP HandleMenuMsg2(UINT, WPARAM, LPARAM, LRESULT* result) override;

private:
    HRESULT HandleString(UINT type, LPSTR name, UINT cchMax, const char* verbA, const wchar_t* verbW, const char* helpA, const wchar_t* helpW);

    enum : UINT {
        kCmdExplore = 0,
        kCmdProperties = 1,
        kCmdUnload = 2,
        kCmdCopyPath = 3,
        kCmdCount = 4
    };

    std::vector<ContextMenuItemData> items_;
    PIDLIST_ABSOLUTE folderPidl_ = nullptr;
};
