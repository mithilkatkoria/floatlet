#include "services/account.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
using namespace delight::account;
int main(int argc,char** argv){
    winrt::init_apartment();int failures=0;
    if(argc>1&&std::strcmp(argv[1],"--live")==0){
        try{
            auto path=std::getenv("FLOATLET_AUTH_TEST_CONFIG"),email=std::getenv("FLOATLET_AUTH_TEST_EMAIL"),password=std::getenv("FLOATLET_AUTH_TEST_PASSWORD");
            if(!path||!email||!password)throw std::runtime_error("Live account test configuration is missing.");
            auto config=Config::load(path);if(!config)throw std::runtime_error("Live account config not found.");
            Client client(*config);auto session=client.signIn(std::wstring(winrt::to_hstring(email)),std::wstring(winrt::to_hstring(password)));
            if(!session.usable()||session.email!=std::wstring(winrt::to_hstring(email)))throw std::runtime_error("Server identity verification failed.");
            auto refreshed=client.refresh(session);if(!refreshed.usable())throw std::runtime_error("Session refresh failed.");
            client.signOut(refreshed);bool revoked=false;try{client.refresh(refreshed);}catch(const Error& error){revoked=!error.network;}
            if(!revoked)throw std::runtime_error("Signed-out session could still refresh.");
            std::cout<<"Live email sign-in, identity, refresh and logout checks passed.\n";return 0;
        }catch(const std::exception& error){std::cerr<<"Live account check failed: "<<error.what()<<"\n";return 1;}catch(...){std::cerr<<"Live account check failed.\n";return 1;}
    }
    auto check=[&](bool result,const char* label){if(!result){++failures;std::cerr<<label<<"\n";}};
    auto config=[](const wchar_t* url,const wchar_t* key){JsonObject json;json.Insert(L"url",JsonValue::CreateStringValue(url));json.Insert(L"publishableKey",JsonValue::CreateStringValue(key));return Config::parse(json);};
    check(config(L"https://example.supabase.co",L"sb_publishable_test").host==L"example.supabase.co","Valid public config");
    for(auto url:{L"http://example.supabase.co",L"https://example.supabase.co.evil.test",L"https://example.supabase.co/path",L"https://.supabase.co"}){
        bool rejected=false;try{config(url,L"sb_publishable_test");}catch(...){rejected=true;}check(rejected,"Invalid service origin accepted");
    }
    for(auto key:{L"sb_secret_test",L"sb_publishable_",L"sb_publishable_test\r\nInjected: yes"}){
        bool rejected=false;try{config(L"https://example.supabase.co",key);}catch(...){rejected=true;}check(rejected,"Invalid client key accepted");
    }
    check(challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk")=="E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM","RFC 7636 PKCE challenge");
    check(random().size()==43,"Secure verifier length");
    check(callbackCode("GET /floatlet/nonce?code=abc-123 HTTP/1.1\r\nHost: localhost\r\n\r\n","/floatlet/nonce")==std::optional<std::string>{"abc-123"},"Valid callback");
    check(!callbackCode("GET /floatlet/wrong?code=abc-123 HTTP/1.1\r\n","/floatlet/nonce"),"Callback nonce mismatch accepted");
    check(!callbackCode("GET /floatlet/nonce?error=denied HTTP/1.1\r\n","/floatlet/nonce"),"Error callback accepted");
    check(!callbackCode("GET /floatlet/nonce?code=abc%0d%0a HTTP/1.1\r\n","/floatlet/nonce"),"Encoded callback injection accepted");
    auto folder=std::filesystem::temp_directory_path()/winrt::to_hstring("floatlet-auth-"+random(12)).c_str();
    auto file=folder/L"session.dat";Store store(file);Session expected{L"test-access",L"test-refresh",L"test@example.invalid",now()+3600};
    store.save(expected);auto loaded=store.load();check(loaded&&loaded->access==expected.access&&loaded->refresh==expected.refresh&&loaded->usable(),"DPAPI session roundtrip");
    {std::ifstream stream(file,std::ios::binary);std::string encrypted((std::istreambuf_iterator<char>(stream)),{});check(encrypted.find("test-refresh")==std::string::npos,"Refresh token stored in plaintext");}
    {std::ofstream stream(file,std::ios::binary|std::ios::trunc);stream<<"corrupt";}check(!store.load(),"Corrupt session accepted");
    store.clear();check(!store.load(),"Deleted session restored");std::filesystem::remove(folder);
    std::cout<<(failures?"Account checks failed":"Account checks passed")<<"\n";return failures?1:0;
}
