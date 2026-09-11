#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <winrt/base.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
namespace delight {
struct MicrophoneSnapshot {bool active=false,muted=false;std::wstring app;DWORD pid=0;};
class MicrophoneMonitor {
    class Signal final:public IMMNotificationClient,public IAudioSessionNotification,public IAudioSessionEvents,public IAudioEndpointVolumeCallback {
        std::atomic<ULONG> refs{1};HANDLE wake;std::mutex callbackLock;
        void changed(){std::scoped_lock lock(callbackLock);if(wake)SetEvent(wake);}
    public:
        explicit Signal(HANDLE h):wake(h){}
        void stop(){std::scoped_lock lock(callbackLock);wake=nullptr;}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==__uuidof(IMMNotificationClient))*out=static_cast<IMMNotificationClient*>(this);else if(id==__uuidof(IAudioSessionNotification))*out=static_cast<IAudioSessionNotification*>(this);else if(id==__uuidof(IAudioSessionEvents))*out=static_cast<IAudioSessionEvents*>(this);else if(id==__uuidof(IAudioEndpointVolumeCallback))*out=static_cast<IAudioEndpointVolumeCallback*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
        ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow,ERole,LPCWSTR)override{if(flow==eCapture)changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR,DWORD)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR,const PROPERTYKEY)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl*)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR,LPCGUID)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR,LPCGUID)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float,BOOL,LPCGUID)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD,float[],DWORD,LPCGUID)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID,LPCGUID)override{return S_OK;}
        HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason)override{changed();return S_OK;}
        HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA)override{changed();return S_OK;}
    };
    HWND hwnd;HANDLE wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);std::atomic_bool stopping=false,enabled=true,toggle=false;std::thread worker;std::mutex mutex;MicrophoneSnapshot snapshot;
    static std::wstring appName(DWORD pid){HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return {};wchar_t path[32768]{};DWORD size=32768;bool ok=QueryFullProcessImageNameW(process,0,path,&size)!=FALSE;CloseHandle(process);if(!ok)return {};std::wstring name(path,size);auto slash=name.find_last_of(L"\\/");if(slash!=std::wstring::npos)name=name.substr(slash+1);std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return wchar_t(towlower(c));});for(auto pair:{std::pair{L"discord",L"Discord"},std::pair{L"whatsapp",L"WhatsApp"},std::pair{L"telegram",L"Telegram"},std::pair{L"teams",L"Teams"},std::pair{L"zoom",L"Zoom"},std::pair{L"skype",L"Skype"},std::pair{L"snapchat",L"Snapchat"}})if(name.find(pair.first)!=std::wstring::npos)return pair.second;if(name==L"chrome.exe"||name==L"msedge.exe"||name==L"firefox.exe")return L"Browser microphone";return {};}
    void run(){winrt::init_apartment(winrt::apartment_type::multi_threaded);auto signal=new Signal(wake);winrt::com_ptr<IMMDeviceEnumerator> enumerator;std::vector<winrt::com_ptr<IAudioSessionManager2>> managers;std::vector<winrt::com_ptr<IAudioSessionControl>> sessions;winrt::com_ptr<IAudioEndpointVolume> endpoint;
        auto clear=[&]{if(endpoint)endpoint->UnregisterControlChangeNotify(signal);endpoint=nullptr;for(auto& s:sessions)s->UnregisterAudioSessionNotification(signal);sessions.clear();for(auto& m:managers)m->UnregisterSessionNotification(signal);managers.clear();};
        if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(enumerator.put()))))enumerator->RegisterEndpointNotificationCallback(signal);
        SetEvent(wake);while(WaitForSingleObject(wake,INFINITE)==WAIT_OBJECT_0&&!stopping){
            if(toggle.exchange(false)&&endpoint){BOOL muted=FALSE;if(SUCCEEDED(endpoint->GetMute(&muted)))endpoint->SetMute(!muted,nullptr);}
            clear();MicrophoneSnapshot next;
            if(enabled&&enumerator){winrt::com_ptr<IMMDeviceCollection> devices;if(SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture,DEVICE_STATE_ACTIVE,devices.put()))){UINT count=0;devices->GetCount(&count);
                for(UINT i=0;i<std::min(count,8u);++i){winrt::com_ptr<IMMDevice> device;if(FAILED(devices->Item(i,device.put())))continue;winrt::com_ptr<IAudioSessionManager2> manager;if(FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_INPROC_SERVER,nullptr,manager.put_void())))continue;manager->RegisterSessionNotification(signal);managers.push_back(manager);winrt::com_ptr<IAudioSessionEnumerator> list;if(FAILED(manager->GetSessionEnumerator(list.put())))continue;int length=0;list->GetCount(&length);
                    for(int j=0;j<std::min(length,64);++j){winrt::com_ptr<IAudioSessionControl> session;if(FAILED(list->GetSession(j,session.put())))continue;session->RegisterAudioSessionNotification(signal);sessions.push_back(session);AudioSessionState state{};if(next.active||FAILED(session->GetState(&state))||state!=AudioSessionStateActive)continue;auto detail=session.try_as<IAudioSessionControl2>();DWORD pid=0;if(!detail||FAILED(detail->GetProcessId(&pid)))continue;auto name=appName(pid);if(name.empty())continue;
                        if(SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_INPROC_SERVER,nullptr,endpoint.put_void()))){BOOL muted=FALSE;endpoint->GetMute(&muted);endpoint->RegisterControlChangeNotify(signal);next={true,muted!=FALSE,std::move(name),pid};}
                    }
                }
            }}
            bool changed=false;{std::scoped_lock lock(mutex);changed=next.active!=snapshot.active||next.muted!=snapshot.muted||next.app!=snapshot.app||next.pid!=snapshot.pid;snapshot=std::move(next);}if(changed)PostMessageW(hwnd,WM_APP+14,0,0);
        }
        signal->stop();clear();if(enumerator)enumerator->UnregisterEndpointNotificationCallback(signal);signal->Release();
    }
public:
    MicrophoneMonitor(HWND h,bool on):hwnd(h),enabled(on){if(!wake)winrt::throw_last_error();worker=std::thread([this]{run();});}
    ~MicrophoneMonitor(){stopping=true;SetEvent(wake);worker.join();CloseHandle(wake);}
    void enable(bool on){enabled=on;SetEvent(wake);}
    void mute(){toggle=true;SetEvent(wake);}
    MicrophoneSnapshot get(){std::scoped_lock lock(mutex);return snapshot;}
};
}
