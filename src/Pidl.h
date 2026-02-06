#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>

// PIDL = Pointer to an ID list - opaque data used by our extension and Explorer to identify items.
//
// An absolute PIDL is a fullly qualified path to the item.
// A relative PIDL is relative to the item's parent folder.
namespace Pidl {
constexpr DWORD kSignature = 0x4C444F4D; // 'MODL'

PIDLIST_ABSOLUTE CreateRoot();
PIDLIST_RELATIVE CreateFromPath(const std::wstring& path, void* baseAddress, DWORD size);
PIDLIST_RELATIVE Clone(PCUIDLIST_RELATIVE pidl);
void Free(PIDLIST_RELATIVE pidl);
bool IsOurPidl(PCUIDLIST_RELATIVE pidl);
std::wstring GetPath(PCUIDLIST_RELATIVE pidl);
void* GetBaseAddress(PCUIDLIST_RELATIVE pidl);
DWORD GetSize(PCUIDLIST_RELATIVE pidl);
} // namespace Pidl
