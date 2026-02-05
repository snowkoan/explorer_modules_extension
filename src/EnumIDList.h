#pragma once

#include <windows.h>
#include <shlobj.h>
#include <wrl.h>
#include <vector>

class EnumIDList final
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IEnumIDList> {
public:
    explicit EnumIDList(const std::vector<PIDLIST_RELATIVE>& items);
    ~EnumIDList();

    IFACEMETHODIMP Next(ULONG celt, PITEMID_CHILD* rgelt, ULONG* fetched) override;
    IFACEMETHODIMP Skip(ULONG celt) override;
    IFACEMETHODIMP Reset() override;
    IFACEMETHODIMP Clone(IEnumIDList** ppenum) override;

private:
    std::vector<PIDLIST_RELATIVE> items_;
    ULONG index_ = 0;
};
