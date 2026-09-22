#include "search/model.h"
#include "search/placement.h"
#include "services/media_brand.h"
#include "search/everything_protocol.h"
#include <iostream>
#include <cstdlib>
using namespace delight::search;
int main(){int checks=0;auto check=[&](bool yes){++checks;if(!yes){std::cerr<<"Search check failed: "<<checks<<'\n';std::exit(1);}};
    for(unsigned dpi:{96u,120u,144u,168u,192u,288u}) {
        for(auto work:{Placement{0,0,1920,1080},Placement{-2560,-400,2560,1440},Placement{1920,-1920,1080,1920},Placement{0,0,640,480}}) {
            auto box=searchPlacement(work.x,work.y,work.x+work.width,work.y+work.height,dpi);
            check(box.width>0&&box.height>0);
            check(box.x>=work.x&&box.y>=work.y);
            check(box.x+box.width<=work.x+work.width&&box.y+box.height<=work.y+work.height);
            check(std::abs((box.x-work.x)-(work.width-box.width)/2)<=1);
        }
    }
    check(delight::appleMusicName(L"AppleInc.AppleMusicWin"));check(delight::appleMusicName(L"Apple Music - Web Player - Google Chrome"));check(!delight::appleMusicName(L"YouTube - Google Chrome"));check(delight::mediaBrowser(L"Chrome")==L"chrome");check(delight::mediaBrowser(L"Spotify.exe").empty());
    Result folder{L"folder",L"Certificates",L"C:\\Documents",L"",Kind::Folder};
    Result file{L"file",L"Maths Certificate.pdf",L"C:\\Documents\\Certificates",L"",Kind::File};
    Result child{L"child",L"Scan.png",L"C:\\Documents\\Certificates",L"",Kind::File};
    check(rank(folder,L"cert")>rank(child,L"cert"));check(rank(folder,L"certificates")>rank(file,L"certificates"));
    check(rank(file,L"maths pdf")>0);check(rank(child,L"certificates")>0);check(rank(file,L"documents certificate")>0);
    check(rank(folder,L"certifciates")>0);check(rank(folder,L"zzzzzz")==0);check(rank(file,L"passport")==0);
    check(distance(L"certifciates",L"certificates")==1);check(distance(L"abc",L"abcdefgh")>2);
    check(fold(L"C:/TEMP")==L"c:\\temp");check(tokens(L"  maths  pdf ").size()==2);
    check(applicationTarget(L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App")==L"shell:AppsFolder\\Microsoft.WindowsNotepad_8wekyb3d8bbwe!App");
    check(applicationTarget(L"Chrome")==L"shell:AppsFolder\\Chrome");check(applicationTarget(L"C:\\Apps\\Example.exe")==L"C:\\Apps\\Example.exe");check(applicationTarget(L"shell:AppsFolder\\Chrome")==L"shell:AppsFolder\\Chrome");
    Result chrome;chrome.title=L"Google Chrome";Result googleFolder;googleFolder.title=L"Google";googleFolder.kind=Kind::Folder;check(rank(chrome,L"google")>rank(googleFolder,L"google"));check(rank(chrome,L"unrelated")==0);
    Result unicode{L"unicode",L"数学 Certificate.pdf",L"C:\\Documents",L"",Kind::File};check(rank(unicode,L"数学")>0);
    Result cafe;cafe.title=L"Café.pdf";check(rank(cafe,L"CAFÉ")>0);
    Result code;code.title=L"Visual Studio Code";code.aliases=L"vsc vscode";check(rank(code,L"vscode")>0);
    Result app;app.title=L"Notepad";auto sameNameFolder=app;sameNameFolder.kind=Kind::Folder;check(rank(app,L"Notepad")>rank(sameNameFolder,L"Notepad"));
    check(rank(file,L"")==0);Result command{L"cmd",L"Calendar",L"",L"",Kind::Command};check(rank(command,L"")>0);
    std::vector<Result> results;for(int i=0;i<600;++i){auto r=file;r.id=std::to_wstring(i);r.score=i;results.push_back(r);}sort(results);check(results.size()==80);check(results.front().score==599);
    for(int i=0;i<100;++i){check(rank(folder,L"cert")>0);check(rank(folder,L"certifciates")>0);}
    std::vector<unsigned char> packet(40);
    auto put=[&](size_t at,std::uint32_t value){std::memcpy(packet.data()+at,&value,4);};
    auto append=[&](const std::wstring& text){auto offset=std::uint32_t(packet.size());for(auto c:text){packet.push_back(static_cast<unsigned char>(c));packet.push_back(static_cast<unsigned char>(c>>8));}packet.push_back(0);packet.push_back(0);return offset;};
    put(20,1);put(28,1);auto nameOffset=append(L"Certificates"),pathOffset=append(L"C:\\Documents");put(32,nameOffset);put(36,pathOffset);
    auto parsed=decodeEverythingReply(packet.data(),packet.size());check(parsed&&parsed->size()==1);check(parsed&&parsed->front().kind==Kind::Folder&&parsed->front().target==L"C:\\Documents\\Certificates");
    check(!decodeEverythingReply(nullptr,28));check(!decodeEverythingReply(packet.data(),27));check(!decodeEverythingReply(packet.data(),EverythingReplyLimit+1));
    put(20,513);check(!decodeEverythingReply(packet.data(),packet.size()));put(20,1);
    put(32,29);check(!decodeEverythingReply(packet.data(),packet.size()));put(32,41);check(!decodeEverythingReply(packet.data(),packet.size()));put(32,0xfffffff0);check(!decodeEverythingReply(packet.data(),packet.size()));put(32,nameOffset);
    check(!decodeEverythingReply(packet.data(),packet.size()-2));put(20,0);check(decodeEverythingReply(packet.data(),28)->empty());
    std::cout<<checks<<" search checks passed\n";
}
