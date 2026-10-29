#pragma once
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shobjidl.h>
#include <filesystem>
#include <unordered_map>
#include "model.h"
namespace delight::search {
class Icons {
    std::unordered_map<std::wstring,std::shared_ptr<void>> cache;
    IImageList* images=nullptr;
public:
    ~Icons(){if(images)images->Release();}
    void fill(Result& result){
        std::wstring key=result.id;
        if(result.kind==Kind::File)key=L"extension:"+fold(std::filesystem::path(result.target).extension().wstring());
        if(result.kind==Kind::Folder)key=L"folder";
        if(result.kind==Kind::Command||result.kind==Kind::Window)return;
        if(auto it=cache.find(key);it!=cache.end()){result.icon=it->second;return;}
        SHFILEINFOW info{};
        UINT flags=SHGFI_SYSICONINDEX|SHGFI_LARGEICON;
        bool found=false;
        if(result.kind==Kind::Application){
            PIDLIST_ABSOLUTE pidl=nullptr;
            auto target=applicationTarget(result.target);
            if(SUCCEEDED(SHParseDisplayName(target.c_str(),nullptr,&pidl,0,nullptr))){
                found=SHGetFileInfoW(reinterpret_cast<LPCWSTR>(pidl),0,&info,sizeof info,flags|SHGFI_PIDL)!=0;
                CoTaskMemFree(pidl);
            }
        }else{
            found=SHGetFileInfoW(result.target.c_str(),result.kind==Kind::Folder?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL,&info,sizeof info,flags|SHGFI_USEFILEATTRIBUTES)!=0;
        }
        HICON icon=nullptr;
        if(found){
            if(!images)SHGetImageList(SHIL_EXTRALARGE,IID_IImageList,reinterpret_cast<void**>(&images));
            if(images)images->GetIcon(info.iIcon,ILD_TRANSPARENT,&icon);
        }
        // AppsFolder items do not always have a system image-list index.
        // Request their own icon directly before showing a generic symbol.
        if(!icon&&result.kind==Kind::Application){
            PIDLIST_ABSOLUTE pidl=nullptr;
            auto target=applicationTarget(result.target);
            if(SUCCEEDED(SHParseDisplayName(target.c_str(),nullptr,&pidl,0,nullptr))){
                SHFILEINFOW direct{};
                if(SHGetFileInfoW(reinterpret_cast<LPCWSTR>(pidl),0,&direct,sizeof direct,SHGFI_PIDL|SHGFI_ICON|SHGFI_LARGEICON))icon=direct.hIcon;
                CoTaskMemFree(pidl);
            }
        }
        if(!icon&&result.kind!=Kind::Application){
            SHFILEINFOW direct{};
            if(SHGetFileInfoW(result.target.c_str(),result.kind==Kind::Folder?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL,&direct,sizeof direct,SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON))icon=direct.hIcon;
        }
        if(icon)result.icon=std::shared_ptr<void>(icon,[](void* value){DestroyIcon(static_cast<HICON>(value));});
        if(cache.size()>=96)cache.clear();if(result.icon)cache.emplace(key,result.icon);
    }
};
}
