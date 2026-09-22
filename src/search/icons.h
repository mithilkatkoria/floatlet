#pragma once
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <filesystem>
#include <unordered_map>
#include "model.h"
namespace delight::search {
class Icons {
    std::unordered_map<std::wstring,std::shared_ptr<void>> cache;
public:
    void fill(Result& result){
        std::wstring key=result.id;
        if(result.kind==Kind::File)key=L"extension:"+fold(std::filesystem::path(result.target).extension().wstring());
        if(result.kind==Kind::Folder)key=L"folder";
        if(result.kind==Kind::Command||result.kind==Kind::Window)return;
        if(auto it=cache.find(key);it!=cache.end()){result.icon=it->second;return;}
        SHFILEINFOW info{};
        if(result.kind==Kind::Application){PIDLIST_ABSOLUTE pidl=nullptr;auto target=applicationTarget(result.target);if(SUCCEEDED(SHParseDisplayName(target.c_str(),nullptr,&pidl,0,nullptr))){SHGetFileInfoW(reinterpret_cast<LPCWSTR>(pidl),0,&info,sizeof info,SHGFI_PIDL|SHGFI_ICON|SHGFI_LARGEICON);CoTaskMemFree(pidl);}}
        else SHGetFileInfoW(result.target.c_str(),result.kind==Kind::Folder?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_NORMAL,&info,sizeof info,SHGFI_USEFILEATTRIBUTES|SHGFI_ICON|SHGFI_LARGEICON);
        if(info.hIcon)result.icon=std::shared_ptr<void>(info.hIcon,[](void* icon){DestroyIcon(static_cast<HICON>(icon));});
        if(cache.size()>=96)cache.clear();if(result.icon)cache.emplace(key,result.icon);
    }
};
}
