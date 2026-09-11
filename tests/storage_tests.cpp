#include "storage/settings.h"
#include <winrt/base.h>
#include <windows.h>
#include <fstream>
#include <iostream>
using namespace delight;
int checks=0;
void require(bool ok){++checks;if(!ok)throw std::runtime_error("Storage assertion failed");}
int main(){
    winrt::init_apartment();
    auto root=std::filesystem::temp_directory_path()/(L"DelightIsland-tests-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    try{
        auto file=root/L"settings.json";Settings s;s.touch=true;s.alignment=2;s.followPointer=false;s.iconOnly=true;
        require(addReference(s,L"C:\\missing\\日本語.txt"));require(!addReference(s,L"c:/missing/日本語.txt"));require(!addReference(s,L"relative.txt"));
        save(file,s);auto restored=load(file);require(restored.paths==s.paths&&restored.touch&&restored.alignment==2&&!restored.followPointer&&restored.iconOnly);
        {std::ofstream out(file);out<<"{bad";}bool bad=false;try{load(file);}catch(...){bad=true;}require(bad);
        {std::ofstream out(file);out<<"{\"version\":2}";}bad=false;try{load(file);}catch(...){bad=true;}require(bad);
        s.paths.clear();for(int i=0;i<100;++i)require(addReference(s,L"C:\\missing\\"+std::to_wstring(i)));
        require(!addReference(s,L"C:\\missing\\overflow"));save(file,s);require(load(file).paths.size()==100);
        auto original=root/L"disposable.txt";{std::ofstream out(original);out<<"Keep this original";}
        for(int i=0;i<100;++i){Settings cycle;require(addReference(cycle,original.wstring()));save(file,cycle);cycle=load(file);cycle.paths.clear();save(file,cycle);require(std::filesystem::exists(original));require(std::filesystem::file_size(original)==18);}
        // Delete only disposable files created by this test, never paths from settings.
        std::filesystem::remove(file);std::filesystem::remove(original);std::filesystem::remove(root);
        std::cout<<checks<<" storage and original-preservation checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<" at check "<<checks<<'\n';return 1;}
}
