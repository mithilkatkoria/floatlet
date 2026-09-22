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
        s.search.enabled=false;s.search.applications=false;s.search.files=false;s.search.windows=false;s.search.fuzzy=false;s.search.key='F';s.search.modifiers=MOD_CONTROL|MOD_SHIFT;
        save(file,s);restored=load(file);require(!restored.search.enabled&&!restored.search.applications&&!restored.search.files&&!restored.search.windows&&!restored.search.fuzzy);require(restored.search.key=='F'&&restored.search.modifiers==(MOD_CONTROL|MOD_SHIFT));
        for(auto shortcut:{std::pair<unsigned,unsigned>{VK_F12,MOD_ALT},{VK_TAB,MOD_ALT},{'F',0},{256,MOD_CONTROL},{'F',8}}){s.search.key=shortcut.first;s.search.modifiers=shortcut.second;save(file,s);restored=load(file);require(restored.search.key==VK_SPACE&&restored.search.modifiers==MOD_ALT);}
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
