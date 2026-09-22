#pragma once
#include <windows.h>
#include <winhttp.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <vector>
#include "media_brand.h"
namespace delight {
struct ArtHttp { HINTERNET value{};~ArtHttp(){if(value)WinHttpCloseHandle(value);} };
inline std::vector<unsigned char> artGet(const std::wstring& url,size_t limit){
    URL_COMPONENTS parts{sizeof parts};parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)return {};
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    if(host!=L"itunes.apple.com"&&!host.ends_with(L".mzstatic.com"))return {};
    ArtHttp session{WinHttpOpen(L"Floatlet/0.6",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0)};if(!session.value)return {};
    WinHttpSetTimeouts(session.value,2000,2000,3000,3000);
    ArtHttp connection{WinHttpConnect(session.value,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0)};if(!connection.value)return {};
    ArtHttp request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};if(!request.value)return {};
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof redirects);
    if(!WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))return {};
    DWORD status=0,size=sizeof status;if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)||status!=200)return {};
    std::vector<unsigned char> bytes;auto start=GetTickCount64();for(;;){DWORD available=0;if(GetTickCount64()-start>6000||!WinHttpQueryDataAvailable(request.value,&available))return {};if(!available)break;if(available>limit-bytes.size())return {};auto before=bytes.size();bytes.resize(before+available);DWORD read=0;if(!WinHttpReadData(request.value,bytes.data()+before,available,&read))return {};bytes.resize(before+read);if(!read)break;}return bytes;
}
inline std::wstring artworkMatchKey(std::wstring text){text=mediaLower(text);std::erase_if(text,[](wchar_t c){return !iswalnum(c);});return text;}
inline std::vector<unsigned char> catalogueArtwork(const std::wstring& title,const std::wstring& artist){
    if(title.empty()||artist.empty()||title.size()>300||artist.size()>300)return {};
    try{
        auto term=winrt::Windows::Foundation::Uri::EscapeComponent(title+L" "+artist);
        auto bytes=artGet(L"https://itunes.apple.com/search?media=music&entity=song&limit=12&term="+std::wstring(term),256*1024);if(bytes.empty())return {};
        auto json=winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(std::string(bytes.begin(),bytes.end())));
        auto results=json.GetNamedArray(L"results");for(auto value:results){auto item=value.GetObject();if(artworkMatchKey(std::wstring(item.GetNamedString(L"trackName",L"")))!=artworkMatchKey(title)||artworkMatchKey(std::wstring(item.GetNamedString(L"artistName",L"")))!=artworkMatchKey(artist))continue;
            auto url=std::wstring(item.GetNamedString(L"artworkUrl100",L""));if(url.starts_with(L"http://"))url.replace(0,7,L"https://");return artGet(url,4*1024*1024);
        }
    }catch(...){}return {};
}
}
