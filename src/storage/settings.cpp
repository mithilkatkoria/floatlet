#include "settings.h"
#include "ui/model.h"
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <fstream>
using namespace winrt::Windows::Data::Json;
namespace delight {
Settings load(const std::filesystem::path& file) {
    Settings s;
    if(!std::filesystem::exists(file)) return s;
    if(std::filesystem::file_size(file)>1024*1024) throw std::runtime_error("Settings exceed 1 MiB");
    std::ifstream in(file,std::ios::binary); std::string raw((std::istreambuf_iterator<char>(in)),{});
    JsonObject root;
    if(!JsonObject::TryParse(winrt::to_hstring(raw),root) || root.GetNamedNumber(L"version",0)!=1) throw std::runtime_error("Unsupported or damaged settings");
    double align=root.GetNamedNumber(L"alignment",1);
    if(align<0 || align>2 || align!=static_cast<int>(align)) throw std::runtime_error("Invalid alignment");
    s.alignment=static_cast<int>(align); s.touch=root.GetNamedBoolean(L"touch",false);
    s.followPointer=root.GetNamedBoolean(L"followPointer",true);
    s.iconOnly=root.GetNamedBoolean(L"iconOnly",false);s.reduceMotion=root.GetNamedBoolean(L"reduceMotion",false);s.callControls=root.GetNamedBoolean(L"callControls",true);
    auto search=root.GetNamedObject(L"search",JsonObject());
    s.search.enabled=search.GetNamedBoolean(L"enabled",true);s.search.applications=search.GetNamedBoolean(L"applications",true);s.search.files=search.GetNamedBoolean(L"files",true);s.search.windows=search.GetNamedBoolean(L"windows",true);s.search.fuzzy=search.GetNamedBoolean(L"fuzzy",true);
    auto key=search.GetNamedNumber(L"key",32),modifiers=search.GetNamedNumber(L"modifiers",1);
    if(key>=1&&key<=255&&key==int(key)&&key!=VK_F12&&modifiers>=1&&modifiers<=7&&modifiers==int(modifiers)&&!(key==VK_TAB&&modifiers==MOD_ALT)){s.search.key=unsigned(key);s.search.modifiers=unsigned(modifiers);}
    auto paths=root.GetNamedArray(L"paths",JsonArray());
    if(paths.Size()>ShelfLimit) throw std::runtime_error("Too many shelf references");
    for(auto&& p:paths) if(!addReference(s,std::wstring(p.GetString()))) throw std::runtime_error("Invalid or duplicate reference");
    return s;
}
bool addReference(Settings& s,const std::wstring& raw) {
    if(raw.empty() || raw.size()>32760 || raw.find(L'\0')!=std::wstring::npos || !std::filesystem::path(raw).is_absolute()) return false;
    auto path=pathKey(raw);
    for(auto& existing:s.paths) if(CompareStringOrdinal(existing.c_str(),-1,path.c_str(),-1,TRUE)==CSTR_EQUAL) return false;
    if(s.paths.size()>=ShelfLimit) return false;
    s.paths.push_back(path); return true;
}
void save(const std::filesystem::path& file,const Settings& s) {
    JsonObject root; root.Insert(L"version",JsonValue::CreateNumberValue(1)); root.Insert(L"alignment",JsonValue::CreateNumberValue(s.alignment)); root.Insert(L"touch",JsonValue::CreateBooleanValue(s.touch));
    root.Insert(L"followPointer",JsonValue::CreateBooleanValue(s.followPointer));
    root.Insert(L"iconOnly",JsonValue::CreateBooleanValue(s.iconOnly));
    root.Insert(L"reduceMotion",JsonValue::CreateBooleanValue(s.reduceMotion));root.Insert(L"callControls",JsonValue::CreateBooleanValue(s.callControls));
    JsonObject search;search.Insert(L"enabled",JsonValue::CreateBooleanValue(s.search.enabled));search.Insert(L"applications",JsonValue::CreateBooleanValue(s.search.applications));search.Insert(L"files",JsonValue::CreateBooleanValue(s.search.files));search.Insert(L"windows",JsonValue::CreateBooleanValue(s.search.windows));search.Insert(L"fuzzy",JsonValue::CreateBooleanValue(s.search.fuzzy));search.Insert(L"key",JsonValue::CreateNumberValue(s.search.key));search.Insert(L"modifiers",JsonValue::CreateNumberValue(s.search.modifiers));root.Insert(L"search",search);
    JsonArray paths; for(auto& path:s.paths) paths.Append(JsonValue::CreateStringValue(path)); root.Insert(L"paths",paths);
    std::string raw=winrt::to_string(root.Stringify()); if(raw.size()>1024*1024) throw std::runtime_error("Settings exceed limit");
    std::filesystem::create_directories(file.parent_path()); auto temp=file; temp+=L".tmp";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc); out.write(raw.data(),raw.size()); out.flush(); if(!out) throw std::runtime_error("Settings write failed");}
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Settings replacement failed");
}
}
