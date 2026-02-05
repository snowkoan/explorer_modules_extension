#include "Pidl.h"
#include "Log.h"

#include <shlobj.h>
#include <cstring>

namespace Pidl {
namespace {
    // Offsets within the data section (after cb)
    constexpr size_t kOffsetSignature = 0;
    constexpr size_t kOffsetBaseAddress = kOffsetSignature + sizeof(DWORD);
    constexpr size_t kOffsetSize = kOffsetBaseAddress + sizeof(UINT64);
    constexpr size_t kOffsetPath = kOffsetSize + sizeof(DWORD);
    constexpr size_t kFixedDataSize = kOffsetPath;

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
} // namespace

PIDLIST_ABSOLUTE CreateRoot() {
    return CreateFromPath(L"", nullptr, 0);
}

PIDLIST_RELATIVE CreateFromPath(const std::wstring& path, void* baseAddress, DWORD size) {
    const size_t pathSize = (path.size() + 1) * sizeof(wchar_t);
    const size_t dataSize = kFixedDataSize + pathSize;
    auto pidl = AllocatePidl(dataSize);
    if (!pidl) {
        Log::Write(Log::Level::Error, L"CreateFromPath allocation failed");
        return nullptr;
    }
    auto bytes = reinterpret_cast<BYTE*>(pidl);
    BYTE* data = bytes + sizeof(USHORT);

    // Signature
    DWORD sig = kSignature;
    memcpy(data + kOffsetSignature, &sig, sizeof(DWORD));

    // Base Address
    UINT64 addr = reinterpret_cast<UINT64>(baseAddress);
    memcpy(data + kOffsetBaseAddress, &addr, sizeof(UINT64));

    // Size
    memcpy(data + kOffsetSize, &size, sizeof(DWORD));

    // Path
    memcpy(data + kOffsetPath, path.c_str(), pathSize);

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
    if (cb < sizeof(USHORT) + kFixedDataSize) {
        return false;
    }
    
    auto bytes = reinterpret_cast<const BYTE*>(pidl);
    DWORD sig = 0;
    memcpy(&sig, bytes + sizeof(USHORT) + kOffsetSignature, sizeof(DWORD));
    return sig == kSignature;
}

PCUIDLIST_RELATIVE GetOurItem(PCUIDLIST_RELATIVE pidl) {
     if (!pidl) {
        return nullptr;
    }
    PCUIDLIST_RELATIVE item = pidl;
    if (!IsOurPidl(item)) {
        auto last = ILFindLastID(reinterpret_cast<PCIDLIST_ABSOLUTE>(pidl));
        if (last && IsOurPidl(last)) {
            item = last;
        } else {
             return nullptr;
        }
    }
    return item;
}

std::wstring GetPath(PCUIDLIST_RELATIVE pidl) {
    auto item = GetOurItem(pidl);
    if (!item) {
        Log::Write(Log::Level::Warn, L"GetPath called with non-module PIDL");
        return L"";
    }

    auto bytes = reinterpret_cast<const BYTE*>(item);
    auto str = reinterpret_cast<const wchar_t*>(bytes + sizeof(USHORT) + kOffsetPath);
    return str;
}

void* GetBaseAddress(PCUIDLIST_RELATIVE pidl) {
    auto item = GetOurItem(pidl);
    if (!item) return nullptr;

    auto bytes = reinterpret_cast<const BYTE*>(item);
    UINT64 addr = 0;
    memcpy(&addr, bytes + sizeof(USHORT) + kOffsetBaseAddress, sizeof(UINT64));
    return reinterpret_cast<void*>(addr);
}

DWORD GetSize(PCUIDLIST_RELATIVE pidl) {
    auto item = GetOurItem(pidl);
    if (!item) return 0;

    auto bytes = reinterpret_cast<const BYTE*>(item);
    DWORD size = 0;
    memcpy(&size, bytes + sizeof(USHORT) + kOffsetSize, sizeof(DWORD));
    return size;
}
} // namespace Pidl
