#include "ModuleFolder.h"
#include "IidNames.h"
#include "Log.h"
#include <wrl.h>
#include <wrl/module.h>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

class ModuleFolderClassFactory final
    : public RuntimeClass<RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IClassFactory> {
public:
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        auto iidText = IidNames::ToString(riid);
        Log::Write(Log::Level::Info, L"ClassFactory::CreateInstance called (riid=%s)", iidText.c_str());
        if (outer != nullptr) {
            Log::Write(Log::Level::Warn, L"ClassFactory::CreateInstance aggregation not supported");
            return CLASS_E_NOAGGREGATION;
        }
        auto folder = Make<ModuleFolder>();
        if (!folder) {
            Log::Write(Log::Level::Error, L"ClassFactory::CreateInstance out of memory");
            return E_OUTOFMEMORY;
        }
        HRESULT hr = folder.CopyTo(riid, ppv);
        if (!SUCCEEDED(hr))
        {
            Log::Write(Log::Level::Error, L"ClassFactory::CreateInstance hr=0x%08X", hr);
        }
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        auto& module = Microsoft::WRL::Module<Microsoft::WRL::InProc>::GetModule();
        if (lock) {
            module.IncrementObjectCount();
        } else {
            module.DecrementObjectCount();
        }
        return S_OK;
    }
};

HRESULT CreateModuleFolderClassFactory(REFIID riid, void** ppv) {
    auto factory = Make<ModuleFolderClassFactory>();
    if (!factory) {
        return E_OUTOFMEMORY;
    }
    return factory.CopyTo(riid, ppv);
}
