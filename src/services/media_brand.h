#pragma once
#include <string>
#include <cwctype>
#include <algorithm>
namespace delight {
inline std::wstring mediaLower(std::wstring text){std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c){return wchar_t(towlower(c));});return text;}
inline bool appleMusicName(const std::wstring& text){auto name=mediaLower(text);return name.find(L"apple music")!=name.npos||name.find(L"applemusic")!=name.npos;}
inline std::wstring mediaBrowser(const std::wstring& source){auto name=mediaLower(source);for(auto browser:{L"chrome",L"msedge",L"firefox",L"brave",L"opera"})if(name.find(browser)!=name.npos)return browser;return {};}
}
