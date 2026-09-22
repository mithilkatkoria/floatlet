#pragma once
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace delight::search {
enum class Kind { Application, Command, File, Folder, Window };
struct Result {
    std::wstring id, title, detail, target;
    Kind kind=Kind::Application;
    int command=0, score=0;
    std::uintptr_t window=0;
    unsigned long process=0;
    std::shared_ptr<void> icon;
    std::wstring activationId,aliases;
};
inline std::wstring applicationTarget(const std::wstring& target) {
    // AppsFolder parsing names can be AppUserModelIDs rather than filesystem paths.
    if(target.empty()||target.starts_with(L"shell:")||target.starts_with(L"::{")||target.starts_with(L"\\\\")||(target.size()>2&&target[1]==L':'))return target;
    return L"shell:AppsFolder\\"+target;
}
inline std::wstring fold(std::wstring s) {
#ifdef _WIN32
    if(!s.empty()){std::wstring mapped(s.size(),L'\0');if(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,s.data(),int(s.size()),mapped.data(),int(mapped.size()),nullptr,nullptr,0))s=std::move(mapped);}
#endif
    for(auto& c:s){c=static_cast<wchar_t>(towlower(c));if(c==L'/')c=L'\\';}return s; }
inline std::vector<std::wstring> tokens(const std::wstring& query) {
    std::vector<std::wstring> out;std::wstring token;
    for(auto c:fold(query)){if(iswspace(c)){if(!token.empty()){out.push_back(token);token.clear();}}else token+=c;}
    if(!token.empty())out.push_back(token);return out;
}
inline int distance(const std::wstring& a,const std::wstring& b,int limit=2) {
    if(a.size()>128||b.size()>128||std::abs(int(a.size())-int(b.size()))>limit)return limit+1;
    std::vector<int> prev(b.size()+1),prior(b.size()+1),row(b.size()+1);
    for(size_t j=0;j<=b.size();++j)prev[j]=int(j);
    for(size_t i=1;i<=a.size();++i){row[0]=int(i);int minimum=row[0];for(size_t j=1;j<=b.size();++j){row[j]=std::min({prev[j]+1,row[j-1]+1,prev[j-1]+(a[i-1]!=b[j-1])});if(i>1&&j>1&&a[i-1]==b[j-2]&&a[i-2]==b[j-1])row[j]=std::min(row[j],prior[j-2]+1);minimum=std::min(minimum,row[j]);}if(minimum>limit)return limit+1;prior.swap(prev);prev.swap(row);}return prev.back();
}
inline int rank(const Result& r,const std::wstring& query,bool fuzzy=true) {
    auto parts=tokens(query);if(parts.empty())return r.kind==Kind::Command?100:0;
    auto name=fold(r.title),path=fold(r.detail),aliases=fold(r.aliases),q=fold(query);int score=0;
    for(auto& token:parts){auto at=name.find(token);if(at==0)score+=600;else if(at!=name.npos)score+=400;else if(!aliases.empty()&&aliases.find(token)!=aliases.npos)score+=520;else if(path.find(token)!=path.npos)score+=180;else {
        bool match=false;if(fuzzy&&token.size()>=4){std::wstring word;auto check=[&]{if(word.size()>=3&&distance(token,word)<=2)match=true;word.clear();};for(auto c:name){if(iswalnum(c))word+=c;else check();}check();}
        if(!match)return 0;score+=70;
    }}
    if(name==q)score+=1000;else if(name.starts_with(q))score+=300;
    if(name==q&&(r.kind==Kind::Application||r.kind==Kind::Command))score+=120;
    if(r.kind==Kind::Application)score+=10000;
    if(r.kind==Kind::Folder&&name.find(q)!=name.npos)score+=40;
    return score;
}
inline void sort(std::vector<Result>& results) {
    std::stable_sort(results.begin(),results.end(),[](auto& a,auto& b){if(a.score!=b.score)return a.score>b.score;if(a.title!=b.title)return a.title<b.title;return a.id<b.id;});
    if(results.size()>80)results.resize(80);
}
}
