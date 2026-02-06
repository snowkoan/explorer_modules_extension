#include "Pidl.h"
#include "Log.h"

#include <shlobj.h>
#include <cstring>
#include <type_traits>

namespace Pidl {
namespace {

// Define the binary structure of our PIDL explicitly.
// #pragma pack(1) ensures no padding bytes are inserted by the compiler.
#pragma pack(push, 1)
struct PidlData {
    DWORD signature;
    UINT64 baseAddress;
    DWORD size;
    // Variable length path string follows this struct
};
#pragma pack(pop)

static_assert(sizeof(PidlData) == 16, "PidlData size mismatch");

// Helper to get pointer to the PidlData struct from a PIDL
const PidlData* GetData(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) return nullptr;
    // PIDL structure: [cb (2 bytes)] [Data...]
    return reinterpret_cast<const PidlData*>(reinterpret_cast<const BYTE*>(pidl) + sizeof(USHORT));
}

// Helper to get variable length path
const wchar_t* GetPathStart(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) return nullptr;
    return reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(pidl) + sizeof(USHORT) + sizeof(PidlData));
}

PIDLIST_RELATIVE AllocatePidl(size_t variableDataSize) {
    const size_t dataSize = sizeof(PidlData) + variableDataSize;
    constexpr size_t cbSize = sizeof(USHORT);
    const size_t totalSize = cbSize + dataSize + sizeof(USHORT); // cb + data + null terminator (for next item 0)
    
    auto pidl = static_cast<PIDLIST_RELATIVE>(CoTaskMemAlloc(totalSize));
    if (!pidl) {
        Log::Write(Log::Level::Error, L"AllocatePidl failed for %zu bytes", totalSize);
        return nullptr;
    }
    
    // Zero init everything (important for the terminator)
    memset(pidl, 0, totalSize);
    
    // Set cb
    pidl->mkid.cb = static_cast<USHORT>(cbSize + dataSize);
    return pidl;
}

} // namespace

PIDLIST_ABSOLUTE CreateRoot() {
    return CreateFromPath(L"", nullptr, 0);
}

PIDLIST_RELATIVE CreateFromPath(const std::wstring& path, void* baseAddress, DWORD size) {
    const size_t pathBytes = (path.size() + 1) * sizeof(wchar_t);
    auto pidl = AllocatePidl(pathBytes);
    
    if (!pidl) {
        return nullptr;
    }

    // Fill data
    auto bytes = reinterpret_cast<BYTE*>(pidl);
    // Use PidlData* for easier assignment, assuming x86/x64 unaligned access is fine.
    auto data = reinterpret_cast<PidlData*>(bytes + sizeof(USHORT));
    
    data->signature = kSignature;
    data->baseAddress = reinterpret_cast<UINT64>(baseAddress);
    data->size = size;
    
    auto pathDest = reinterpret_cast<wchar_t*>(bytes + sizeof(USHORT) + sizeof(PidlData));
    memcpy(pathDest, path.c_str(), pathBytes);

    return pidl;
}

PIDLIST_RELATIVE Clone(PCUIDLIST_RELATIVE pidl) {
    if (!pidl) {
        Log::Write(Log::Level::Warn, L"Clone called with null pidl");
        return nullptr;
    }
    const USHORT cb = pidl->mkid.cb;
    if (cb == 0) return nullptr;

    auto cloneBytes = static_cast<BYTE*>(CoTaskMemAlloc(cb + sizeof(USHORT)));
    if (!cloneBytes) {
        Log::Write(Log::Level::Error, L"Clone allocation failed");
        return nullptr;
    }
    
    memcpy(cloneBytes, pidl, cb);
    // Zero terminate list
    *reinterpret_cast<USHORT*>(cloneBytes + cb) = 0;
    
    return reinterpret_cast<PIDLIST_RELATIVE>(cloneBytes);
}

void Free(PIDLIST_RELATIVE pidl) {
    if (pidl) {
        CoTaskMemFree(pidl);
    }
}

bool IsOurPidl(PCUIDLIST_RELATIVE pidl) {
    if (!pidl || pidl->mkid.cb < sizeof(USHORT) + sizeof(PidlData)) {
        return false;
    }
    
    const PidlData* data = GetData(pidl);
    return data->signature == kSignature;
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
        return L"";
    }
    
    // Validate size again just in case
    if (item->mkid.cb < sizeof(USHORT) + sizeof(PidlData)) return L"";

    return GetPathStart(item);
}

void* GetBaseAddress(PCUIDLIST_RELATIVE pidl) {
    auto item = GetOurItem(pidl);
    if (!item) return nullptr;
    return reinterpret_cast<void*>(GetData(item)->baseAddress);
}

DWORD GetSize(PCUIDLIST_RELATIVE pidl) {
    auto item = GetOurItem(pidl);
    if (!item) return 0;
    return GetData(item)->size;
}
} // namespace Pidl
