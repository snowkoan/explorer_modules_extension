#pragma once

#include <windows.h>
#include <shlobj.h>
#include <string>

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
