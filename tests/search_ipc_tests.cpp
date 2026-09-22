#include "search/everything.h"
#include <winrt/base.h>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace delight::search;
int wmain(int argc,wchar_t** argv){bool slow=argc==2&&std::wstring(argv[1])==L"--slow-probe";bool known=argc==2&&std::wstring(argv[1])==L"--known-name";winrt::init_apartment(winrt::apartment_type::single_threaded);int exitCode=0;{
    auto root=std::filesystem::temp_directory_path()/(L"FloatletSearchFixture"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root/L"Certificates");std::ofstream(root/L"Certificates"/L"Maths Certificate.pdf")<<"Disposable search fixture";
    auto began=GetTickCount64();std::atomic<unsigned> generation=1;Everything ipc;std::vector<Result> found;
    for(int attempt=0;attempt<(slow?1:10)&&found.empty();++attempt){found=ipc.query(known?L"notepad.exe":root.filename().wstring()+L" Certificates",!known,1,generation,false,slow?15000:2500);if(found.empty())Sleep(200);}
    bool folder=false,file=false;for(auto& r:found){folder|=r.title==L"Certificates"&&r.kind==Kind::Folder;file|=r.title==L"Maths Certificate.pdf"&&r.kind==Kind::File;}
    std::cout<<"Everything IPC fixture: folder="<<folder<<" file="<<file<<" returned="<<found.size()<<" elapsed_ms="<<(GetTickCount64()-began)<<'\n';
    std::wcout<<L"Provider: present="<<ipc.present<<L" ready="<<ipc.ready<<L" "<<ipc.status<<L" packets="<<ipc.replyPackets<<L" stale="<<ipc.stalePackets<<L" invalid="<<ipc.invalidPackets<<L'\n';if(known?(!ipc.ready||found.empty()):(!folder||!file))exitCode=1;
    // Only the unique directory created by this test is removed.
    std::filesystem::remove_all(root);
}winrt::uninit_apartment();return exitCode;}
