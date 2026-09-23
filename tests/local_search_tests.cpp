#include "search/local_index.h"
#include "search/shortcut.h"
#include <iostream>
using namespace delight::search;
int checks=0;
void require(bool value){++checks;if(!value)throw std::runtime_error("Local search assertion "+std::to_string(checks));}
int main(int argc,char** argv){if(argc>1&&std::string(argv[1])=="--personal-audit"){std::atomic<unsigned> generation=1;std::wstring status;LocalIndex index;index.configure(true,LocalIndex::locations(0),LocalIndex::cacheFile(0));auto start=GetTickCount64();bool started=false;for(int i=0;i<900;++i){index.query(L"floatlet-audit-no-results-expected",1,generation,status);if(status.find(L"Updating")!=status.npos)started=true;if(started&&status.find(L"Updating")==status.npos){auto scanMs=GetTickCount64()-start;auto warmStart=GetTickCount64();for(int q=0;q<100;++q)index.query(L"floatlet-audit-no-results-expected",1,generation,status);std::wcout<<status<<L" Scan or cache ms: "<<scanMs<<L". Mean warm query ms: "<<double(GetTickCount64()-warmStart)/100<<L"\n";return 0;}Sleep(100);}std::wcerr<<L"Index did not finish within 90 seconds. Last state: "<<status<<L"\n";return 2;}auto root=std::filesystem::temp_directory_path()/(L"Floatlet-index-tests-"+std::to_wstring(GetCurrentProcessId()));auto files=root/L"files";std::filesystem::create_directories(files/L"Projects");try{
    Options o;o.sides=2;require(validShortcut(o));require(matchesShortcut(o,2));require(matchesShortcut(o,6));require(!matchesShortcut(o,1));require(!matchesShortcut(o,3));require(!matchesShortcut(o,18));o.sides=0;require(matchesShortcut(o,1)&&matchesShortcut(o,2));o.key=VK_F12;require(!validShortcut(o));o.key=VK_SPACE;o.sides=8;require(!validShortcut(o));o.modifiers=MOD_CONTROL;require(validShortcut(o));
    {std::ofstream(files/L"Projects"/L"Budget 2026.txt")<<"Original contents";std::ofstream(files/L"Caf\u00e9.txt")<<"Unicode";}
    std::atomic<unsigned> generation=1,notifications=0;std::wstring status;
    auto wait=[&](LocalIndex& index,const std::wstring& query,size_t count){for(int i=0;i<400;++i){auto results=index.query(query,1,generation,status);if(results.size()==count&&status.find(L"Updating")==status.npos)return results;Sleep(10);}throw std::runtime_error("Index timeout");};
    {
        LocalIndex index([&]{++notifications;});index.configure(true,{files.wstring()},root/L"index.bin");
        auto found=wait(index,L"budget",1);require(found[0].title==L"Budget 2026.txt"&&found[0].kind==Kind::File);require(index.query(L"Projects Budget",1,generation,status).size()==1);require(index.query(L"CAF\u00c9",1,generation,status).size()==1);require(index.query(L"Original contents",1,generation,status).empty());require(index.query(L"budget",2,generation,status).empty());require(std::filesystem::exists(root/L"index.bin"));
        std::filesystem::rename(files/L"Projects"/L"Budget 2026.txt",files/L"Projects"/L"Renamed.txt");index.refresh(true);wait(index,L"renamed",1);require(index.query(L"budget",1,generation,status).empty());
        std::filesystem::remove(files/L"Projects"/L"Renamed.txt");require(index.query(L"renamed",1,generation,status).empty());index.refresh(true);wait(index,L"renamed",0);
        index.configure(false);require(index.query(L"caf",1,generation,status).empty());
    }
    {LocalIndex index;index.configure(true,{files.wstring()},root/L"index.bin");require(wait(index,L"caf",1).size()==1);}
    {std::fstream cache(root/L"index.bin",std::ios::binary|std::ios::in|std::ios::out);cache.seekp(8);cache.put(1);require(bool(cache));}
    {LocalIndex index;index.configure(true,{files.wstring()},root/L"index.bin");require(wait(index,L"caf",1).size()==1);require(status.find(L"Index full")!=std::wstring::npos);}
    {std::ofstream out(root/L"index.bin",std::ios::binary|std::ios::trunc);out<<"corrupt";}
    {LocalIndex index;index.configure(true,{files.wstring()},root/L"index.bin");require(wait(index,L"caf",1).size()==1);}
    // Larger fixture exercises capped results and measures query time separately from initial scan.
    for(int i=0;i<3000;++i)std::ofstream(files/(L"item-"+std::to_wstring(i)+L".txt"));
    {LocalIndex index;index.configure(true,{files.wstring()});wait(index,L"item-",80);auto start=GetTickCount64();for(int i=0;i<100;++i)require(!index.query(L"item-29",1,generation,status).empty());std::cout<<"Mean query ms (3,000 files): "<<double(GetTickCount64()-start)/100<<'\n';index.configure(false);}
    require(notifications>=3);require(std::filesystem::exists(files/L"Caf\u00e9.txt"));
    std::filesystem::remove_all(root);std::cout<<checks<<" built-in index and shortcut checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
