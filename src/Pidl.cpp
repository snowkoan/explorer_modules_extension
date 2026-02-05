#include "Pidl.h"
#include "Log.h"

#include <shlobj.h>
#include <cstring>

namespace Pidl {
namespace {
PIDLIST_RELATIVE AllocatePidl(size_t dataSize) {
    const size_t totalSize = sizeof(USHORT) + dataSize + sizeof(USHORT);
    auto pidl = static_cast<PIDLIST_RELATIVE>(CoTaskMemAlloc(totalSize));
    if (!pidl) {
        Log::Write(Log::Level::Error, L"AllocatePidl failed for %zu bytes", totalSize);
        return nullptr;
    }
    auto bytes = reinterpret_cast<BYTE*>(pidl);
    auto cb = static_cast<USHORT>(dataSize + sizeof(USHORT));
    memcpy(bytes, &cb, sizeof(USHORT));
    memset(bytes + sizeof(USHORT), 0, dataSize + sizeof(USHORT));
    return pidl;
}

DWORD* SignaturePtr(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) {
        return nullptr;
    }
    auto bytes = reinterpret_cast<const BYTE*>(pidl);
    return reinterpret_cast<DWORD*>(const_cast<BYTE*>(bytes + sizeof(USHORT)));
}
} // namespace

PIDLIST_ABSOLUTE CreateRoot() {
    return CreateFromPath(L"");
}

PIDLIST_RELATIVE CreateFromPath(const std::wstring& path) {
    const size_t dataSize = sizeof(DWORD) + (path.size() + 1) * sizeof(wchar_t);
    auto pidl = AllocatePidl(dataSize);
    if (!pidl) {
        Log::Write(Log::Level::Error, L"CreateFromPath allocation failed");
        return nullptr;
    }
    auto bytes = reinterpret_cast<BYTE*>(pidl);
    auto signature = reinterpret_cast<DWORD*>(bytes + sizeof(USHORT));
    *signature = kSignature;
    auto str = reinterpret_cast<wchar_t*>(bytes + sizeof(USHORT) + sizeof(DWORD));
    memcpy(str, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
    return pidl;
}

PIDLIST_RELATIVE Clone(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) {
        Log::Write(Log::Level::Warn, L"Clone called with null pidl");
        return nullptr;
    }
    const USHORT cb = pidl->mkid.cb;
    auto clone = static_cast<PIDLIST_RELATIVE>(CoTaskMemAlloc(cb + sizeof(USHORT)));
    if (!clone) {
        Log::Write(Log::Level::Error, L"Clone allocation failed for %u bytes", cb + static_cast<USHORT>(sizeof(USHORT)));
        return nullptr;
    }
    memcpy(clone, pidl, cb + sizeof(USHORT));
    return clone;
}

void Free(PIDLIST_RELATIVE pidl) {
    if (pidl) {
        CoTaskMemFree(pidl);
    }
}

bool IsOurPidl(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) {
        return false;
    }
    const USHORT cb = pidl->mkid.cb;
    if (cb < sizeof(USHORT) + sizeof(DWORD)) {
        return false;
    }
    auto signature = SignaturePtr(pidl);
    return signature && *signature == kSignature;
}

std::wstring GetPath(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) {
        Log::Write(Log::Level::Warn, L"GetPath called with null PIDL");
        return L"";
    }

    PCUIDLIST_RELATIVE item = pidl;
    if (!IsOurPidl(item)) {
        auto last = ILFindLastID(reinterpret_cast<PCIDLIST_ABSOLUTE>(pidl));
        if (last && IsOurPidl(last)) {
            item = last;
        } else {
            Log::Write(Log::Level::Warn, L"GetPath called with non-module PIDL");
            return L"";
        }
    }

    auto bytes = reinterpret_cast<const BYTE*>(item);
    auto str = reinterpret_cast<const wchar_t*>(bytes + sizeof(USHORT) + sizeof(DWORD));
    return str;
}
} // namespace Pidl
