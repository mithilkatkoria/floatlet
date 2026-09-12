#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <winrt/base.h>
#include <atomic>
#include <algorithm>
#include <string>
namespace delight {
// Endpoint notifications carry no borrowed data across threads.
class Audio final : public IMMNotificationClient, public IAudioEndpointVolumeCallback {
    std::atomic<ULONG> refs{1}; std::atomic<HWND> window{nullptr};
    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    winrt::com_ptr<IAudioEndpointVolume> endpoint;
    winrt::com_ptr<IAudioMeterInformation> meter;
    void notify(UINT message){if(auto h=window.load())PostMessageW(h,message,0,0);}
public:
    float level=0; bool muted=false,available=false; std::wstring name,id;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(riid==IID_IUnknown||riid==__uuidof(IMMNotificationClient))*out=static_cast<IMMNotificationClient*>(this);
        else if(riid==__uuidof(IAudioEndpointVolumeCallback))*out=static_cast<IAudioEndpointVolumeCallback*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override {notify(WM_APP+10);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow f,ERole r,LPCWSTR) override {if(f==eRender&&r==eMultimedia)notify(WM_APP+9);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD) override {notify(WM_APP+9);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override{notify(WM_APP+9);return S_OK;}
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY) override{return S_OK;}
    void start(HWND h){window=h;if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(enumerator.put())))){enumerator->RegisterEndpointNotificationCallback(this);bind();}}
    void bind(){
        if(endpoint)endpoint->UnregisterControlChangeNotify(this);endpoint=nullptr;meter=nullptr;available=false;name.clear();id.clear();
        if(!enumerator)return;winrt::com_ptr<IMMDevice> device;
        if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,device.put())))return;
        device->Activate(__uuidof(IAudioMeterInformation),CLSCTX_INPROC_SERVER,nullptr,meter.put_void());
        LPWSTR raw=nullptr;if(SUCCEEDED(device->GetId(&raw))){id=raw;CoTaskMemFree(raw);}
        winrt::com_ptr<IPropertyStore> properties;
        if(SUCCEEDED(device->OpenPropertyStore(STGM_READ,properties.put()))){PROPVARIANT v{};if(SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName,&v))&&v.vt==VT_LPWSTR&&v.pwszVal)name=v.pwszVal;PropVariantClear(&v);}
        if(SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_INPROC_SERVER,nullptr,endpoint.put_void()))){endpoint->RegisterControlChangeNotify(this);read();}
    }
    void read(){BOOL m=FALSE;available=endpoint&&SUCCEEDED(endpoint->GetMasterVolumeLevelScalar(&level))&&SUCCEEDED(endpoint->GetMute(&m));muted=m!=FALSE;}
    bool set(float value){if(!endpoint)return false;bool ok=SUCCEEDED(endpoint->SetMasterVolumeLevelScalar(std::clamp(value,0.f,1.f),nullptr));read();return ok;}
    bool toggle(){if(!endpoint)return false;read();bool ok=SUCCEEDED(endpoint->SetMute(!muted,nullptr));read();return ok;}
    float peak(){float value=0;if(meter)meter->GetPeakValue(&value);return std::clamp(value,0.f,1.f);}
    bool airpods() const {auto n=name;std::transform(n.begin(),n.end(),n.begin(),[](wchar_t c){return wchar_t(towlower(c));});return n.find(L"airpods")!=std::wstring::npos;}
    void stop(){window=nullptr;meter=nullptr;if(endpoint)endpoint->UnregisterControlChangeNotify(this);endpoint=nullptr;if(enumerator)enumerator->UnregisterEndpointNotificationCallback(this);enumerator=nullptr;}
};
}
