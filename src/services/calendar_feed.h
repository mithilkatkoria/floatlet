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
    void load(){try{if(!std::filesystem::exists(file)||std::filesystem::file_size(file)>131072)return;std::ifstream stream(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};if(CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)){if(output.cbData%sizeof(wchar_t)==0)url.assign(reinterpret_cast<wchar_t*>(output.pbData),output.cbData/sizeof(wchar_t));LocalFree(output.pbData);try{parseLinks(url);}catch(...){url.clear();}}}catch(...){url.clear();}snapshot.connected=!url.empty();}
    void run(){
        struct Cached {ical::Calendar calendar;ULONGLONG fetched=0;};
        std::map<std::wstring,Cached> caches;
        for(;;){std::wstring request;std::chrono::year_month_day date;unsigned version;bool reload;
            {std::unique_lock lock(mutex);if(url.empty())cv.wait(lock,[&]{return done||pending;});else cv.wait_for(lock,std::chrono::minutes{1},[&]{return done||pending;});if(done)return;pending=false;request=url;date=selected;version=generation;reload=force;force=false;}
            auto links=parseLinks(request);
            std::erase_if(caches,[&](auto& item){return std::find(links.begin(),links.end(),item.first)==links.end();});
            AgendaSnapshot next;next.year=int(date.year());next.month=unsigned(date.month());next.day=unsigned(date.day());next.connected=!links.empty();
            unsigned fresh=0,failed=0;
            auto today=std::chrono::floor<std::chrono::days>(std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));
            for(auto& link:links){
                {std::scoped_lock lock(mutex);if(done)return;if(version!=generation)break;}
                auto& cache=caches[link];bool online=true;
                try{if(reload||!cache.fetched||GetTickCount64()-cache.fetched>=55*1000){cache.calendar=ical::parse(download(link));cache.fetched=GetTickCount64();}++fresh;}
                catch(...){online=false;++failed;}
                if(!cache.fetched)continue;
                try{auto day=ical::day(cache.calendar,date);next.agenda.unsupported+=day.unsupported;mergeEntries(next.agenda.entries,day.entries);
                    if(online)for(int d=0;d<2;++d){auto events=ical::day(cache.calendar,std::chrono::year_month_day{today+std::chrono::days{d}});std::erase_if(events.entries,[](auto& e){return e.allDay;});mergeEntries(next.upcoming,events.entries);}
                }catch(...){++next.agenda.unsupported;}
            }
            if(!links.empty())next.status=failed?(fresh?L"Some calendars offline. Available calendars synced.":L"Offline. Showing last loaded events."):(std::to_wstring(links.size())+L" calendar(s) synced. Checks every minute.");
            if(!failed&&next.agenda.unsupported)next.status=L"Synced. Some calendar rules are unsupported.";
            {std::scoped_lock lock(mutex);if(done)return;if(version!=generation)continue;snapshot=std::move(next);}PostMessageW(hwnd,WM_APP+13,0,0);
        }
    }
public:
    static bool validUrl(const std::wstring& value){return value.size()<=4096&&value.starts_with(L"https://calendar.google.com/calendar/ical/")&&value.find(L".ics")!=std::wstring::npos&&value.find_first_of(L"\r\n\t ")==std::wstring::npos&&value.find(L'\0')==std::wstring::npos&&value.find(L'#')==std::wstring::npos;}
    static constexpr size_t MaxCalendars=8;
    static std::vector<std::wstring> parseLinks(const std::wstring& value){
        std::vector<std::wstring> links;std::wistringstream input(value);std::wstring line;
        while(std::getline(input,line)){auto first=line.find_first_not_of(L" \t\r");if(first==std::wstring::npos)continue;line=line.substr(first,line.find_last_not_of(L" \t\r")-first+1);
            if(!validUrl(line))throw std::runtime_error("Use Google Calendar HTTPS iCal links");
            if(std::find(links.begin(),links.end(),line)==links.end())links.push_back(line);
            if(links.size()>MaxCalendars)throw std::runtime_error("Up to eight calendars are supported");
        }return links;
    }
    static std::wstring joinLinks(const std::vector<std::wstring>& links){std::wstring text;for(auto& link:links){if(!text.empty())text+=L"\n";text+=link;}return text;}
    static void mergeEntries(std::vector<ical::Entry>& target,const std::vector<ical::Entry>& source){
        for(auto& e:source){bool duplicate=std::any_of(target.begin(),target.end(),[&](auto& existing){return existing.start==e.start&&existing.allDay==e.allDay&&(!e.uid.empty()?existing.uid==e.uid:existing.uid.empty()&&existing.title==e.title);});if(!duplicate)target.push_back(e);}
        std::stable_sort(target.begin(),target.end(),[](auto& a,auto& b){if(a.allDay!=b.allDay)return a.allDay;return a.start<b.start;});
    }
    std::vector<std::wstring> connections(){std::scoped_lock lock(mutex);return parseLinks(url);}
    CalendarFeed(HWND h,std::filesystem::path folder):hwnd(h),file(std::move(folder)/L"calendar.bin"){load();worker=std::thread([this]{run();});}
    ~CalendarFeed(){{std::scoped_lock lock(mutex);done=true;}cv.notify_one();worker.join();}
    AgendaSnapshot get(){std::scoped_lock lock(mutex);return snapshot;}
    void connect(const std::wstring& value){auto normalized=joinLinks(parseLinks(value));store(normalized);{std::scoped_lock lock(mutex);url=normalized;++generation;snapshot={};snapshot.connected=!url.empty();} }
    void request(int year,unsigned month,unsigned day,bool refresh=false){std::chrono::year_month_day date{std::chrono::year{year},std::chrono::month{month},std::chrono::day{day}};if(!date.ok())return;{std::scoped_lock lock(mutex);selected=date;pending=true;force|=refresh;}cv.notify_one();}
};
}
