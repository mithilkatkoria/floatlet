#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <wlanapi.h>
#include <shlobj.h>
#include <winrt/base.h>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>
#include <algorithm>
namespace delight {
struct WifiProfile {GUID adapter{};std::wstring name,profile;unsigned signal=0;bool connected=false,secured=false;};
struct ControlSnapshot {unsigned revision=0;HRESULT brightnessError=S_OK;int brightness=-1;std::vector<WifiProfile> wifi;std::vector<std::wstring> screenshots;std::wstring status;};
// One sleeping worker. WMI, filesystem metadata and WLAN never run on the UI thread.
class SystemControls {
    HWND hwnd;std::mutex mutex;std::condition_variable cv;bool done=false;
    bool refreshLight=false,refreshWifi=false,refreshShots=false,scanWifi=false;
    std::optional<int> light;std::optional<WifiProfile> connection;
    ControlSnapshot snapshot;std::thread worker;
    struct Bstr {BSTR p;explicit Bstr(const wchar_t* s):p(SysAllocString(s)){}~Bstr(){SysFreeString(p);}operator BSTR() const{return p;}};
    static winrt::com_ptr<IWbemServices> wmi(){winrt::com_ptr<IWbemLocator> locator;winrt::check_hresult(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(locator.put())));winrt::com_ptr<IWbemServices> service;winrt::check_hresult(locator->ConnectServer(Bstr(L"ROOT\\WMI"),nullptr,nullptr,nullptr,0,nullptr,nullptr,service.put()));winrt::check_hresult(CoSetProxyBlanket(service.get(),RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE));return service;}
    static winrt::com_ptr<IWbemClassObject> query(IWbemServices* service,const wchar_t* text){winrt::com_ptr<IEnumWbemClassObject> values;winrt::check_hresult(service->ExecQuery(Bstr(L"WQL"),Bstr(text),WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,values.put()));winrt::com_ptr<IWbemClassObject> row;ULONG n=0;winrt::check_hresult(values->Next(2000,1,row.put(),&n));if(!n)throw std::runtime_error("No brightness provider");return row;}
    static int brightness(std::optional<int> set){
        auto service=wmi();
        if(set){auto row=query(service.get(),L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE");VARIANT path{};winrt::check_hresult(row->Get(L"__PATH",0,&path,nullptr,nullptr));
            try{winrt::com_ptr<IWbemClassObject> cls,in,params,out;winrt::check_hresult(service->GetObject(Bstr(L"WmiMonitorBrightnessMethods"),0,nullptr,cls.put(),nullptr));winrt::check_hresult(cls->GetMethod(L"WmiSetBrightness",0,in.put(),nullptr));winrt::check_hresult(in->SpawnInstance(0,params.put()));VARIANT v{};v.vt=VT_UI1;v.bVal=BYTE(std::clamp(*set,0,100));winrt::check_hresult(params->Put(L"Brightness",0,&v,0));VARIANT timeout{};timeout.vt=VT_BSTR;timeout.bstrVal=SysAllocString(L"0");auto hr=params->Put(L"Timeout",0,&timeout,0);VariantClear(&timeout);winrt::check_hresult(hr);winrt::check_hresult(service->ExecMethod(path.bstrVal,Bstr(L"WmiSetBrightness"),0,nullptr,params.get(),out.put(),nullptr));if(out){VARIANT code{};auto result=out->Get(L"ReturnValue",0,&code,nullptr,nullptr);bool success=result==WBEM_E_NOT_FOUND||(SUCCEEDED(result)&&((code.vt==VT_I4&&code.lVal==0)||(code.vt==VT_UI4&&code.ulVal==0)||code.vt==VT_EMPTY||code.vt==VT_NULL));VariantClear(&code);if(!success)throw std::runtime_error("Brightness rejected");}}catch(...){VariantClear(&path);throw;}VariantClear(&path);
        }
        auto row=query(service.get(),L"SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active=TRUE");VARIANT value{};winrt::check_hresult(row->Get(L"CurrentBrightness",0,&value,nullptr,nullptr));int result=value.vt==VT_I4?value.lVal:value.vt==VT_UI1?value.bVal:-1;VariantClear(&value);return result;
    }
    static void wifi(ControlSnapshot& s,const std::optional<WifiProfile>& connect,bool scan){
        HANDLE client=nullptr;DWORD version=0;auto code=WlanOpenHandle(2,nullptr,&version,&client);s.wifi.clear();if(code!=ERROR_SUCCESS){s.status=L"Wi-Fi service unavailable";return;}
        s.status=L"Nearby networks. Scroll for more.";
        if(connect){WLAN_CONNECTION_PARAMETERS p{};p.wlanConnectionMode=wlan_connection_mode_profile;p.strProfile=connect->profile.c_str();p.dot11BssType=dot11_BSS_type_any;code=WlanConnect(client,&connect->adapter,&p,nullptr);s.status=code==ERROR_SUCCESS?L"Connection requested":L"Could not connect. Use Windows Wi-Fi.";}
        PWLAN_INTERFACE_INFO_LIST interfaces=nullptr;bool denied=false,radioOff=false;
        if(WlanEnumInterfaces(client,nullptr,&interfaces)==ERROR_SUCCESS){for(DWORD i=0;i<interfaces->dwNumberOfItems&&s.wifi.size()<30;++i){auto& adapter=interfaces->InterfaceInfo[i];
            if(scan){auto result=WlanScan(client,&adapter.InterfaceGuid,nullptr,nullptr,nullptr);denied|=result==ERROR_ACCESS_DENIED;}
            PWLAN_AVAILABLE_NETWORK_LIST networks=nullptr;auto result=WlanGetAvailableNetworkList(client,&adapter.InterfaceGuid,0,nullptr,&networks);
            denied|=result==ERROR_ACCESS_DENIED;radioOff|=result==ERROR_NDIS_DOT11_POWER_STATE_INVALID;
            if(result==ERROR_SUCCESS){for(DWORD j=0;j<networks->dwNumberOfItems&&s.wifi.size()<30;++j){auto& n=networks->Network[j];if(!n.uNumberOfBssids||!n.dot11Ssid.uSSIDLength||n.dot11Ssid.uSSIDLength>32)continue;
                WifiProfile item;item.adapter=adapter.InterfaceGuid;item.profile=n.strProfileName;item.signal=n.wlanSignalQuality;item.connected=(n.dwFlags&WLAN_AVAILABLE_NETWORK_CONNECTED)!=0;item.secured=n.bSecurityEnabled!=FALSE;
                wchar_t name[65]{};int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,reinterpret_cast<const char*>(n.dot11Ssid.ucSSID),n.dot11Ssid.uSSIDLength,name,64);if(length)item.name.assign(name,length);else item.name=L"Network with non-UTF8 name";for(auto& c:item.name)if(c<32)c=L' ';auto duplicate=std::find_if(s.wifi.begin(),s.wifi.end(),[&](auto& existing){return IsEqualGUID(existing.adapter,item.adapter)&&existing.name==item.name&&existing.secured==item.secured;});if(duplicate==s.wifi.end())s.wifi.push_back(std::move(item));else if(item.connected||(!item.profile.empty()&&duplicate->profile.empty()))*duplicate=std::move(item);}
                WlanFreeMemory(networks);
            }
        }WlanFreeMemory(interfaces);}
        std::stable_sort(s.wifi.begin(),s.wifi.end(),[](auto& a,auto& b){return a.connected!=b.connected?a.connected:a.signal>b.signal;});
        if(denied)s.status=L"Windows location permission is required";else if(s.wifi.empty())s.status=radioOff?L"Wi-Fi is off. Turn it on in Windows.":L"No nearby networks. Refresh to scan again.";
        WlanCloseHandle(client,nullptr);
    }
    static void screenshots(ControlSnapshot& s){
        s.screenshots.clear();PWSTR raw=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Screenshots,0,nullptr,&raw)))return;std::filesystem::path folder(raw);CoTaskMemFree(raw);
        std::vector<std::pair<std::filesystem::file_time_type,std::wstring>> files;std::error_code error;unsigned scanned=0;
        for(auto it=std::filesystem::directory_iterator(folder,error);!error&&it!=std::filesystem::directory_iterator{}&&scanned<4096;it.increment(error),++scanned){if(!it->is_regular_file(error))continue;auto ext=it->path().extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(towlower(c));});if(ext!=L".png"&&ext!=L".jpg"&&ext!=L".jpeg")continue;auto time=it->last_write_time(error);if(error)break;files.emplace_back(time,it->path().wstring());if(files.size()>20){auto oldest=std::min_element(files.begin(),files.end());files.erase(oldest);}}
        std::sort(files.rbegin(),files.rend());for(auto& entry:files)s.screenshots.push_back(std::move(entry.second));
    }
    void run(){winrt::init_apartment(winrt::apartment_type::multi_threaded);for(;;){bool b,w,s,scan;std::optional<int> level;std::optional<WifiProfile> connect;ControlSnapshot next;
        {std::unique_lock lock(mutex);cv.wait(lock,[&]{return done||refreshLight||refreshWifi||refreshShots||light||connection;});if(done)return;b=refreshLight;w=refreshWifi;s=refreshShots;scan=scanWifi;scanWifi=false;refreshLight=refreshWifi=refreshShots=false;level=light;light.reset();connect=std::move(connection);connection.reset();next=snapshot;}
        if(b||level)try{next.brightness=brightness(level);next.brightnessError=S_OK;}catch(const winrt::hresult_error& e){next.brightness=-1;next.brightnessError=e.code();}catch(...){next.brightness=-1;next.brightnessError=E_FAIL;}
        if(w||connect)try{wifi(next,connect,scan);}catch(...){next.status=L"Wi-Fi unavailable";}
        if(s)try{screenshots(next);}catch(...){next.screenshots.clear();}
        {std::scoped_lock lock(mutex);++next.revision;snapshot=std::move(next);if(done)return;}PostMessageW(hwnd,WM_APP+11,0,0);
    }}
public:
    explicit SystemControls(HWND h):hwnd(h),worker([this]{run();}){}
    ~SystemControls(){{std::scoped_lock lock(mutex);done=true;}cv.notify_one();worker.join();}
    ControlSnapshot get(){std::scoped_lock lock(mutex);return snapshot;}
    void refresh(int which){{std::scoped_lock lock(mutex);if(which==0)refreshLight=true;else if(which==1||which==3){refreshWifi=true;scanWifi|=which==1;}else refreshShots=true;}cv.notify_one();}
    void setBrightness(int value){{std::scoped_lock lock(mutex);light=std::clamp(value,0,100);}cv.notify_one();}
    void connect(WifiProfile p){{std::scoped_lock lock(mutex);connection=std::move(p);}cv.notify_one();}
};
}
