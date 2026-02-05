#include "IidNames.h"

#include <objbase.h>
#include <shlobj.h>
#include <strsafe.h>

namespace IidNames {
namespace {
const wchar_t* LookupName(REFIID iid) {
    if (IsEqualIID(iid, IID_IUnknown)) return L"IUnknown";
    if (IsEqualIID(iid, IID_IClassFactory)) return L"IClassFactory";
    if (IsEqualIID(iid, IID_IShellFolder)) return L"IShellFolder";
    if (IsEqualIID(iid, IID_IShellFolder2)) return L"IShellFolder2";
    if (IsEqualIID(iid, IID_IPersist)) return L"IPersist";
    if (IsEqualIID(iid, IID_IPersistFolder)) return L"IPersistFolder";
    if (IsEqualIID(iid, IID_IPersistFolder2)) return L"IPersistFolder2";
    if (IsEqualIID(iid, IID_IDropTarget)) return L"IDropTarget";
    if (IsEqualIID(iid, IID_IShellView)) return L"IShellView";
    if (IsEqualIID(iid, IID_IDataObject)) return L"IDataObject";
    if (IsEqualIID(iid, IID_IEnumIDList)) return L"IEnumIDList";
    return nullptr;
}
} // namespace

std::wstring ToString(REFIID iid) {
    wchar_t iidText[64] = {};
    if (StringFromGUID2(iid, iidText, ARRAYSIZE(iidText)) == 0) {
        lstrcpyW(iidText, L"<invalid>");
    }

    const wchar_t* name = LookupName(iid);
    if (!name) {
        return iidText;
    }

    wchar_t combined[128] = {};
    StringCchPrintfW(combined, ARRAYSIZE(combined), L"%s (%s)", name, iidText);
    return combined;
}
} // namespace IidNames
