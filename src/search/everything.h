#pragma once
#include <windows.h>
#include <atomic>
#include <string>
#include <vector>
#include "model.h"
namespace delight::search {
// Fixed-width wire layout follows the documented Everything Unicode IPC protocol.
// HWND is deliberately serialized as 32 bits, including on ARM64 and x64.
class Everything {
    HWND window{},server{};unsigned serial=0,expected=0;bool complete=false;
    std::vector<Result> received;
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
public:
    struct Installation {std::wstring executable;bool lite=false;};
    static Installation installation();
    Everything();~Everything();
    bool ready=false,present=false;
    unsigned replyPackets=0,stalePackets=0,invalidPackets=0;
    std::wstring status=L"Everything is not running or IPC is unavailable";
    std::vector<Result> query(const std::wstring&,bool path,unsigned,const std::atomic<unsigned>&,bool approximate=false,unsigned timeoutMs=10000);
};
}
