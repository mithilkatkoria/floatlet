#pragma once
#include "ical.h"
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <winrt/base.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
namespace delight {
struct AgendaSnapshot {ical::Agenda agenda;std::vector<ical::Entry> upcoming;std::wstring status=L"Connect a calendar in Preferences";int year=0;unsigned month=0,day=0;bool connected=false;};
class CalendarFeed {
    struct Internet {HINTERNET value=nullptr;explicit Internet(HINTERNET h):value(h){if(!h)winrt::throw_last_error();}~Internet(){WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
    HWND hwnd;std::filesystem::path file;std::mutex mutex;std::condition_variable cv;bool done=false,pending=false,force=false;
    std::wstring url;unsigned generation=0;std::chrono::year_month_day selected{std::chrono::year{2026}/1/1};AgendaSnapshot snapshot;std::thread worker;
    static std::string download(const std::wstring& url){
        if(!validUrl(url))throw std::runtime_error("Invalid Google subscription URL");
        auto path=url.substr(std::wstring(L"https://calendar.google.com").size());
        Internet session(WinHttpOpen(L"Floatlet/0.6",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));WinHttpSetTimeouts(session,5000,5000,5000,5000);
        Internet connection(WinHttpConnect(session,L"calendar.google.com",INTERNET_DEFAULT_HTTPS_PORT,0));
        Internet request(WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES;WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled));
        if(!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr))winrt::throw_last_error();DWORD status=0,bytes=sizeof(status);if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&bytes,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("Calendar server rejected request");
        std::string body;char buffer[8192];DWORD count=0;for(;;){if(!WinHttpReadData(request,buffer,sizeof(buffer),&count))winrt::throw_last_error();if(!count)break;if(body.size()+count>2*1024*1024)throw std::runtime_error("Calendar exceeds 2 MiB");body.append(buffer,count);}if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,body.data(),int(body.size()),nullptr,0))throw std::runtime_error("Calendar is not valid UTF-8");return body;
    }
    void store(const std::wstring& value){
        if(value.empty()){std::error_code error;std::filesystem::remove(file,error);if(error)throw std::runtime_error("Calendar subscription could not be removed");return;}
        DATA_BLOB input{DWORD(value.size()*sizeof(wchar_t)),reinterpret_cast<BYTE*>(const_cast<wchar_t*>(value.data()))},output{};if(!CryptProtectData(&input,L"Floatlet calendar",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))winrt::throw_last_error();
        try{std::filesystem::create_directories(file.parent_path());auto temp=file;temp+=L".tmp";{std::ofstream stream(temp,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<char*>(output.pbData),output.cbData);stream.flush();if(!stream)throw std::runtime_error("Calendar settings write failed");}if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))winrt::throw_last_error();}catch(...){LocalFree(output.pbData);throw;}LocalFree(output.pbData);
    }
    void load(){try{if(!std::filesystem::exists(file)||std::filesystem::file_size(file)>16384)return;std::ifstream stream(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};if(CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)){if(output.cbData%sizeof(wchar_t)==0)url.assign(reinterpret_cast<wchar_t*>(output.pbData),output.cbData/sizeof(wchar_t));LocalFree(output.pbData);if(!validUrl(url))url.clear();}}catch(...){url.clear();}snapshot.connected=!url.empty();}
    void run(){ical::Calendar cache;ULONGLONG fetched=0;unsigned cachedGeneration=~0u;
        for(;;){std::wstring request;std::chrono::year_month_day date;unsigned version;bool reload;
            {std::unique_lock lock(mutex);if(url.empty())cv.wait(lock,[&]{return done||pending;});else cv.wait_for(lock,std::chrono::minutes{1},[&]{return done||pending;});if(done)return;pending=false;request=url;date=selected;version=generation;reload=force;force=false;}
            AgendaSnapshot next;next.year=int(date.year());next.month=unsigned(date.month());next.day=unsigned(date.day());next.connected=!request.empty();
            if(version!=cachedGeneration){cache={};fetched=0;cachedGeneration=version;}
            if(!request.empty())try{if(reload||!fetched||GetTickCount64()-fetched>=55*1000){cache=ical::parse(download(request));fetched=GetTickCount64();}next.agenda=ical::day(cache,date);auto today=std::chrono::floor<std::chrono::days>(std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));for(int d=0;d<2;++d){auto dayEvents=ical::day(cache,std::chrono::year_month_day{today+std::chrono::days{d}});for(auto& entry:dayEvents.entries)if(!entry.allDay)next.upcoming.push_back(entry);}next.status=next.agenda.unsupported?L"Some calendar rules are not supported":L"Synced. Checks for changes every minute.";}
            catch(...){next.status=L"Could not refresh calendar. Check its link.";if(fetched&&cachedGeneration==version)try{next.agenda=ical::day(cache,date);auto today=std::chrono::floor<std::chrono::days>(std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));for(int d=0;d<2;++d){auto dayEvents=ical::day(cache,std::chrono::year_month_day{today+std::chrono::days{d}});for(auto& entry:dayEvents.entries)if(!entry.allDay)next.upcoming.push_back(entry);}next.status=L"Offline. Showing the last loaded calendar.";}catch(...) {}}
            {std::scoped_lock lock(mutex);if(done)return;if(version!=generation)continue;snapshot=std::move(next);}PostMessageW(hwnd,WM_APP+13,0,0);
        }
    }
public:
    static bool validUrl(const std::wstring& value){return value.size()<=4096&&value.starts_with(L"https://calendar.google.com/calendar/ical/")&&value.find(L".ics")!=std::wstring::npos&&value.find_first_of(L"\r\n\t ")==std::wstring::npos&&value.find(L'\0')==std::wstring::npos&&value.find(L'#')==std::wstring::npos;}
    CalendarFeed(HWND h,std::filesystem::path folder):hwnd(h),file(std::move(folder)/L"calendar.bin"){load();worker=std::thread([this]{run();});}
    ~CalendarFeed(){{std::scoped_lock lock(mutex);done=true;}cv.notify_one();worker.join();}
    AgendaSnapshot get(){std::scoped_lock lock(mutex);return snapshot;}
    void connect(const std::wstring& value){if(!value.empty()&&!validUrl(value))throw std::runtime_error("Use a Google Calendar HTTPS iCal link");store(value);{std::scoped_lock lock(mutex);url=value;++generation;snapshot={};snapshot.connected=!url.empty();} }
    void request(int year,unsigned month,unsigned day,bool refresh=false){std::chrono::year_month_day date{std::chrono::year{year},std::chrono::month{month},std::chrono::day{day}};if(!date.ok())return;{std::scoped_lock lock(mutex);selected=date;pending=true;force|=refresh;}cv.notify_one();}
};
}
