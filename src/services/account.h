#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace delight::account {
using winrt::Windows::Data::Json::JsonObject;
using winrt::Windows::Data::Json::JsonValue;
inline int64_t now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
struct Error:std::runtime_error{bool network;Error(const std::string& message,bool offline=false):std::runtime_error(message),network(offline){}};
struct Config{
    std::wstring host,key;bool googleEnabled=false;
    static Config parse(const JsonObject& json){
        auto url=std::wstring(json.GetNamedString(L"url",L""));auto key=std::wstring(json.GetNamedString(L"publishableKey",L""));
        const std::wstring prefix=L"https://",suffix=L".supabase.co";
        if(url.ends_with(L"/"))url.pop_back();
        if(!url.starts_with(prefix)||!url.ends_with(suffix))throw Error("Invalid Floatlet account service URL.");
        auto ref=url.substr(prefix.size(),url.size()-prefix.size()-suffix.size());
        if(ref.empty()||ref.size()>64||ref.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789-")!=std::wstring::npos)throw Error("Invalid Floatlet account service URL.");
        if(!key.starts_with(L"sb_publishable_")||key.size()<=15||key.size()>512||key.find_first_of(L"\r\n\t ")!=std::wstring::npos)throw Error("Use a Supabase publishable key, never an admin secret.");
        return {url.substr(prefix.size()),key,json.GetNamedBoolean(L"googleEnabled",false)};
    }
    static std::optional<Config> load(const std::filesystem::path& file){
        if(!std::filesystem::exists(file))return {};
        if(std::filesystem::file_size(file)>16384)throw Error("Floatlet account configuration is too large.");
        std::ifstream stream(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});
        try{return parse(JsonObject::Parse(winrt::to_hstring(bytes)));}catch(const Error&){throw;}catch(...){throw Error("Floatlet account configuration could not be read.");}
    }
};
struct Session{
    std::wstring access,refresh,email;int64_t expires=0;
    bool usable()const{return !access.empty()&&!refresh.empty()&&!email.empty()&&expires>now()+30;}
};
class Store{
    std::filesystem::path file;
public:
    explicit Store(std::filesystem::path path):file(std::move(path)){}
    void save(const Session& session)const{
        JsonObject json;json.Insert(L"access",JsonValue::CreateStringValue(session.access));json.Insert(L"refresh",JsonValue::CreateStringValue(session.refresh));json.Insert(L"email",JsonValue::CreateStringValue(session.email));json.Insert(L"expires",JsonValue::CreateNumberValue(double(session.expires)));
        auto bytes=winrt::to_string(json.Stringify());DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};
        if(!CryptProtectData(&input,L"Floatlet account",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw Error("Windows could not protect the account session.");
        try{std::filesystem::create_directories(file.parent_path());auto temporary=file;temporary+=L".tmp";{std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<char*>(output.pbData),output.cbData);stream.flush();if(!stream)throw Error("The account session could not be saved.");}
            if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Error("The account session could not be saved.");
        }catch(...){LocalFree(output.pbData);throw;}LocalFree(output.pbData);
    }
    std::optional<Session> load()const{
        try{
            if(!std::filesystem::exists(file)||std::filesystem::file_size(file)>131072)return {};
            std::ifstream stream(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(stream)),{});DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};
            if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))return {};
            std::string plain(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
            auto json=JsonObject::Parse(winrt::to_hstring(plain));SecureZeroMemory(plain.data(),plain.size());
            Session s{std::wstring(json.GetNamedString(L"access")),std::wstring(json.GetNamedString(L"refresh")),std::wstring(json.GetNamedString(L"email")),int64_t(json.GetNamedNumber(L"expires"))};
            if(s.access.size()>32768||s.refresh.size()>4096||s.email.size()>320)return {};return s;
        }catch(...){return {};}
    }
    void clear()const{std::error_code error;std::filesystem::remove(file,error);if(error)throw Error("The account session could not be removed.");}
};
inline std::string base64url(const BYTE* bytes,DWORD size){
    DWORD length=0;if(!CryptBinaryToStringA(bytes,size,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&length))throw Error("Account encryption could not be initialized.");
    std::string result(length,'\0');if(!CryptBinaryToStringA(bytes,size,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,result.data(),&length))throw Error("Account encryption could not be initialized.");
    result.resize(length);while(!result.empty()&&(result.back()=='\0'||result.back()=='='))result.pop_back();std::replace(result.begin(),result.end(),'+','-');std::replace(result.begin(),result.end(),'/','_');return result;
}
inline std::string random(unsigned count=32){std::vector<BYTE> bytes(count);if(BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw Error("Secure account login is unavailable.");return base64url(bytes.data(),DWORD(bytes.size()));}
inline std::string challenge(const std::string& verifier){BYTE hash[32]{};DWORD length=sizeof hash;if(!CryptHashCertificate2(BCRYPT_SHA256_ALGORITHM,0,nullptr,reinterpret_cast<const BYTE*>(verifier.data()),DWORD(verifier.size()),hash,&length))throw Error("Secure account login is unavailable.");return base64url(hash,length);}
inline std::string encode(const std::string& value){static const char hex[]="0123456789ABCDEF";std::string out;for(unsigned char c:value){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;}
inline std::optional<std::string> callbackCode(const std::string& request,const std::string& path){
    auto end=request.find("\r\n");auto line=request.substr(0,end);auto prefix="GET "+path+"?";if(!line.starts_with(prefix))return {};
    auto version=line.find(" HTTP/1.");if(version==std::string::npos)return {};
    auto query=line.substr(prefix.size(),version-prefix.size());size_t offset=0;
    while(offset<query.size()){auto next=query.find('&',offset);auto pair=query.substr(offset,next==std::string::npos?next:next-offset);if(pair.starts_with("code=")){auto code=pair.substr(5);if(code.empty()||code.size()>512||code.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~")!=std::string::npos)return {};return code;}if(next==std::string::npos)break;offset=next+1;}return {};
}
class Client{
    Config config;
    struct Internet{HINTERNET value{};explicit Internet(HINTERNET h):value(h){if(!h)throw Error("The account service could not be reached.",true);}~Internet(){WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
    JsonObject request(const wchar_t* method,const std::wstring& path,const JsonObject& payload={},const std::wstring& token={})const{
        Internet session(WinHttpOpen(L"Floatlet/0.7",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0));WinHttpSetTimeouts(session,3000,3000,4000,4000);
        Internet connection(WinHttpConnect(session,config.host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));Internet req(WinHttpOpenRequest(connection,method,path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
        DWORD disabled=WINHTTP_DISABLE_REDIRECTS|WINHTTP_DISABLE_COOKIES;WinHttpSetOption(req,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof disabled);
        auto headers=L"Content-Type: application/json\r\napikey: "+config.key+L"\r\n";if(!token.empty())headers+=L"Authorization: Bearer "+token+L"\r\n";
        auto body=wcscmp(method,L"GET")==0?std::string{}:winrt::to_string(payload.Stringify());if(!WinHttpSendRequest(req,headers.c_str(),DWORD(headers.size()),body.empty()?nullptr:body.data(),DWORD(body.size()),DWORD(body.size()),0)||!WinHttpReceiveResponse(req,nullptr))throw Error("The account service could not be reached. Check your connection.",true);
        DWORD status=0,size=sizeof status;if(!WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr))throw Error("The account service returned an invalid response.");
        if(status==204)return JsonObject{};
        std::string response;char buffer[8192];DWORD read=0;for(;;){if(!WinHttpReadData(req,buffer,sizeof buffer,&read))throw Error("The account response was interrupted. Please try again.",true);if(!read)break;if(response.size()+read>1048576)throw Error("The account service response was too large.");response.append(buffer,read);}
        JsonObject json;try{json=JsonObject::Parse(winrt::to_hstring(response));}catch(...){throw Error("The account service returned an invalid response.");}
        if(status<200||status>=300){auto message=json.GetNamedString(L"msg",json.GetNamedString(L"error_description",L"Sign-in could not be completed. Please try again."));throw Error(winrt::to_string(message),status>=500);}
        return json;
    }
    Session verified(const JsonObject& json)const{
        Session s{std::wstring(json.GetNamedString(L"access_token",L"")),std::wstring(json.GetNamedString(L"refresh_token",L"")),{},now()+int64_t(json.GetNamedNumber(L"expires_in",0))};
        if(s.access.empty()||s.refresh.empty()||s.access.size()>32768||s.refresh.size()>4096||s.expires<=now())throw Error("The account service did not issue a valid session.");
        auto user=request(L"GET",L"/auth/v1/user",{},s.access);s.email=user.GetNamedString(L"email",L"");
        if(user.GetNamedString(L"id",L"").empty()||user.GetNamedString(L"email_confirmed_at",L"").empty()||s.email.empty())throw Error("Confirm your email before signing in.");
        return s;
    }
public:
    explicit Client(Config value):config(std::move(value)){}
    Session signIn(const std::wstring& email,const std::wstring& password)const{JsonObject body;body.Insert(L"email",JsonValue::CreateStringValue(email));body.Insert(L"password",JsonValue::CreateStringValue(password));return verified(request(L"POST",L"/auth/v1/token?grant_type=password",body));}
    std::optional<Session> signUp(const std::wstring& email,const std::wstring& password)const{JsonObject body;body.Insert(L"email",JsonValue::CreateStringValue(email));body.Insert(L"password",JsonValue::CreateStringValue(password));auto result=request(L"POST",L"/auth/v1/signup?redirect_to=https%3A%2F%2Ffloatlet.vercel.app%2Faccount%2F",body);if(result.GetNamedString(L"access_token",L"").empty())return {};return verified(result);}
    Session refresh(const Session& previous)const{JsonObject body;body.Insert(L"refresh_token",JsonValue::CreateStringValue(previous.refresh));return verified(request(L"POST",L"/auth/v1/token?grant_type=refresh_token",body));}
    void signOut(const Session& session)const{request(L"POST",L"/auth/v1/logout?scope=local",{},session.access);}
    Session google(const std::atomic<bool>& cancel)const{
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))throw Error("Browser sign-in could not start.");
        struct Cleanup{~Cleanup(){WSACleanup();}}cleanup;
        struct Socket{SOCKET value=INVALID_SOCKET;explicit Socket(SOCKET s):value(s){}~Socket(){if(value!=INVALID_SOCKET)closesocket(value);}}listener(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
        BOOL exclusive=TRUE;setsockopt(listener.value,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof exclusive);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(listener.value==INVALID_SOCKET||bind(listener.value,reinterpret_cast<sockaddr*>(&address),sizeof address)||listen(listener.value,2))throw Error("The secure local sign-in callback could not start.");
        int addressSize=sizeof address;getsockname(listener.value,reinterpret_cast<sockaddr*>(&address),&addressSize);
        auto verifier=random(),path="/floatlet/"+random(16);auto redirect="http://127.0.0.1:"+std::to_string(ntohs(address.sin_port))+path;
        auto url=L"https://"+config.host+L"/auth/v1/authorize?provider=google&redirect_to="+winrt::to_hstring(encode(redirect))+L"&code_challenge="+winrt::to_hstring(challenge(verifier))+L"&code_challenge_method=s256";
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)throw Error("The browser could not be opened.");
        auto deadline=GetTickCount64()+180000;
        while(!cancel&&GetTickCount64()<deadline){
            fd_set readSet;FD_ZERO(&readSet);FD_SET(listener.value,&readSet);timeval wait{0,100000};if(select(0,&readSet,nullptr,nullptr,&wait)<=0)continue;
            Socket peer(accept(listener.value,nullptr,nullptr));if(peer.value==INVALID_SOCKET)continue;DWORD timeout=1500;setsockopt(peer.value,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof timeout);
            std::string input;char bytes[2048];int count=0;while(input.find("\r\n\r\n")==std::string::npos&&input.size()<8192&&(count=recv(peer.value,bytes,sizeof bytes,0))>0)input.append(bytes,count);
            auto code=callbackCode(input,path);
            if(!code){const char invalid[]="HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";send(peer.value,invalid,sizeof invalid-1,0);continue;}
            try{
                JsonObject body;body.Insert(L"auth_code",JsonValue::CreateStringValue(winrt::to_hstring(*code)));body.Insert(L"code_verifier",JsonValue::CreateStringValue(winrt::to_hstring(verifier)));auto result=verified(request(L"POST",L"/auth/v1/token?grant_type=pkce",body));
                const char success[]="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n<!doctype html><title>Floatlet</title><p>Signed in to Floatlet. You can close this tab and return to the app.</p>";send(peer.value,success,sizeof success-1,0);return result;
            }catch(...){const char failure[]="HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\nSign-in failed. Return to Floatlet and try again.";send(peer.value,failure,sizeof failure-1,0);throw;}
        }throw Error(cancel?"Sign-in cancelled.":"Browser sign-in timed out. Please try again.");
    }
};
}
