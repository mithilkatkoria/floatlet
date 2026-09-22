#include <windows.h>
#include <shlobj.h>
#include <propkey.h>
#include <winrt/base.h>
#include <iostream>
#include "search/preferences.h"
#include "search/indexed_fallback.h"
#include <chrono>
#include "services/catalogue_art.h"
int main(int argc,char** argv){
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    if(argc==2&&std::string(argv[1])=="--cover"){
        if(delight::artworkMatchKey(L"Ne-Yo")!=delight::artworkMatchKey(L"NE YO"))return 2;
        if(!delight::artGet(L"https://example.com/cover.jpg",1024).empty())return 3;
        auto cover=delight::catalogueArtwork(L"Miss Independent",L"Ne-Yo");
        std::cout<<"Matched cover bytes="<<cover.size()<<'\n';return cover.empty()?1:0;
    }
    if(argc==2&&std::string(argv[1])=="--windows-index"){
        std::atomic<unsigned> generation{1};std::wstring status;
        auto start=std::chrono::steady_clock::now();
        auto results=delight::search::indexedFallback(L"FloatletIndexProbeMissing_91D7",1,generation,status);
        auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        std::wcout<<L"Windows index status="<<status<<L" count="<<results.size()<<L" elapsed_ms="<<elapsed<<L'\n';
        return status==L"Windows indexed files. Everything is unavailable."?0:2;
    }
    if(argc==2&&std::string(argv[1])=="--preferences"){auto result=delight::search::Preferences::show(nullptr,{},false);std::cout<<"Preferences closed; saved="<<result.has_value()<<'\n';return 0;}
    winrt::com_ptr<IShellItem> folder;
    winrt::check_hresult(SHCreateItemInKnownFolder(FOLDERID_AppsFolder,0,nullptr,IID_PPV_ARGS(folder.put())));
    winrt::com_ptr<IEnumShellItems> items;
    winrt::check_hresult(folder->BindToHandler(nullptr,BHID_EnumItems,IID_PPV_ARGS(items.put())));
    unsigned count=0;
    while(count<4096){winrt::com_ptr<IShellItem> item;if(items->Next(1,item.put(),nullptr)!=S_OK)break;++count;
        PWSTR name=nullptr,target=nullptr;
        if(SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&name&&std::wstring(name)==L"Notepad"){
            auto hr=item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,&target);
            std::wcout<<L"Notepad target hr="<<std::hex<<hr<<L" value="<<(target?target:L"unavailable")<<L'\n';
            if(auto rich=item.try_as<IShellItem2>()){PWSTR id=nullptr;hr=rich->GetString(PKEY_AppUserModel_ID,&id);std::wcout<<L"Notepad activation hr="<<std::hex<<hr<<L" value="<<(id?id:L"unavailable")<<L'\n';
                if(id&&argc==2&&std::string(argv[1])=="--activate-notepad"){winrt::com_ptr<IApplicationActivationManager> manager;hr=CoCreateInstance(CLSID_ApplicationActivationManager,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(manager.put()));DWORD process=0;if(SUCCEEDED(hr))hr=manager->ActivateApplication(id,nullptr,AO_NONE,&process);std::cout<<"Activation result="<<std::hex<<hr<<" process="<<std::dec<<process<<'\n';}
                CoTaskMemFree(id);}
        }
        CoTaskMemFree(name);CoTaskMemFree(target);
    }
    std::cout<<"Apps enumerated="<<std::dec<<count<<'\n';
    return count?0:1;
}
