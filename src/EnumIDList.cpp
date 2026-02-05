#include "EnumIDList.h"

#include "Log.h"

#include "Pidl.h"

EnumIDList::EnumIDList(const std::vector<PIDLIST_RELATIVE>& items) {
    items_.reserve(items.size());
    for (auto item : items) {
        auto clone = Pidl::Clone(item);
        if (!clone) {
            Log::Write(Log::Level::Error, L"EnumIDList failed to clone PIDL");
            continue;
        }
        items_.push_back(clone);
    }
}

EnumIDList::~EnumIDList() {
    for (auto item : items_) {
        Pidl::Free(item);
    }
}

IFACEMETHODIMP EnumIDList::Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* fetched) {
    if (!rgelt) {
        return E_POINTER;
    }

    ULONG copied = 0;
    while (copied < celt && index_ < items_.size()) {
        rgelt[copied] = Pidl::Clone(items_[index_]);
        if (!rgelt[copied]) {
            Log::Write(Log::Level::Error, L"EnumIDList::Next failed to clone PIDL");
            break;
        }
        ++copied;
        ++index_;
    }

    if (fetched) {
        *fetched = copied;
    }

    return (copied == celt) ? S_OK : S_FALSE;
}

IFACEMETHODIMP EnumIDList::Skip(ULONG celt) {
    index_ = (index_ + celt > items_.size()) ? static_cast<ULONG>(items_.size()) : index_ + celt;
    return (index_ < items_.size()) ? S_OK : S_FALSE;
}

IFACEMETHODIMP EnumIDList::Reset() {
    index_ = 0;
    return S_OK;
}

IFACEMETHODIMP EnumIDList::Clone(IEnumIDList** ppenum) {
    if (!ppenum) {
        return E_POINTER;
    }
    auto clone = Microsoft::WRL::Make<EnumIDList>(items_);
    if (!clone) {
        return E_OUTOFMEMORY;
    }
    clone->index_ = index_;
    return clone.CopyTo(ppenum);
}
