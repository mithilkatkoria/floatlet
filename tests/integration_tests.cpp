#include "platform/shelf.h"
#include "services/audio.h"
#include "services/system_controls.h"
#include "ui/calendar.h"
#include <iostream>
using namespace delight;
int checks=0;
void require(bool value){++checks;if(!value)throw std::runtime_error("Integration assertion failed");}
int main(int argc,char**){OleInitialize(nullptr);try{
    Calendar calendar;for(int y=2000;y<2400;++y)for(unsigned m=1;m<=12;++m){calendar.year=y;calendar.month=m;calendar.selected=31;unsigned found=0;for(int cell=0;cell<42;++cell)if(calendar.dayAt(cell))++found;require(found==calendar.count());calendar.move(1);require(calendar.selected<=calendar.count());calendar.move(-1);require(calendar.year==y&&calendar.month==m);}
    calendar.year=2024;calendar.month=2;require(calendar.count()==29);calendar.year=2100;require(calendar.count()==28);calendar.year=2026;calendar.month=12;calendar.move(1);require(calendar.year==2027&&calendar.month==1);
    winrt::com_ptr<IDataObject> data;const std::wstring path=L"C:\\Disposable test\\Unicode 日本語.txt";
    require(SUCCEEDED(createFileData(path,data.put())));FORMATETC fmt{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM first{},second{};
    require(data->QueryGetData(&fmt)==S_OK);require(data->GetData(&fmt,&first)==S_OK);require(data->GetData(&fmt,&second)==S_OK);require(first.hGlobal!=second.hGlobal);
    auto raw=static_cast<BYTE*>(GlobalLock(first.hGlobal));auto header=reinterpret_cast<DROPFILES*>(raw);auto text=reinterpret_cast<wchar_t*>(raw+header->pFiles);require(header->fWide&&std::wstring(text)==path&&text[path.size()+1]==0);GlobalUnlock(first.hGlobal);ReleaseStgMedium(&first);ReleaseStgMedium(&second);
    auto target=new DropTarget;bool accepted=false,hover=false;target->hovering=[&](bool yes){hover=yes;};target->accepted=[&](auto paths){accepted=paths.size()==1&&paths[0]==path;};target->rejected=[]{};
    DWORD effect=DROPEFFECT_COPY;target->DragEnter(data.get(),0,{},&effect);require(hover&&effect==DROPEFFECT_COPY);target->Drop(data.get(),0,{},&effect);require(accepted&&!hover&&effect==DROPEFFECT_COPY);
    accepted=false;effect=DROPEFFECT_MOVE;target->DragEnter(data.get(),0,{},&effect);require(effect==DROPEFFECT_NONE);target->Drop(data.get(),0,{},&effect);require(!accepted&&effect==DROPEFFECT_NONE);target->Release();
    fmt.cfFormat=CF_TEXT;require(data->QueryGetData(&fmt)==DV_E_FORMATETC);
    std::cout<<checks<<" calendar and OLE payload assertions passed\n";
    auto audio=new Audio;audio->start(nullptr);std::wcout<<L"Audio endpoint available="<<audio->available<<L", level="<<audio->level<<L", muted="<<audio->muted<<L", AirPods="<<audio->airpods()<<L'\n';
    if(argc>1&&audio->available){float before=audio->level;bool muted=audio->muted;require(audio->set(before));require(std::abs(audio->level-before)<.001f&&audio->muted==muted);std::cout<<"Same-value volume write/read passed\n";}audio->stop();audio->Release();
    {SystemControls system(nullptr);system.refresh(0);system.refresh(3);system.refresh(2);ControlSnapshot snapshot;
        for(int i=0;i<100;++i){std::this_thread::sleep_for(std::chrono::milliseconds(100));snapshot=system.get();if(!snapshot.status.empty())break;}
        std::wcout<<L"Built-in brightness="<<snapshot.brightness<<L", nearby Wi-Fi networks="<<snapshot.wifi.size()<<L", saved screenshots="<<snapshot.screenshots.size()<<L'\n';
        if(argc>1&&snapshot.brightness>=0){int before=snapshot.brightness;unsigned revision=snapshot.revision;system.setBrightness(before);for(int i=0;i<100;++i){std::this_thread::sleep_for(std::chrono::milliseconds(100));snapshot=system.get();if(snapshot.revision>revision)break;}std::cout<<"Brightness result="<<snapshot.brightness<<" HRESULT="<<std::hex<<snapshot.brightnessError<<std::dec<<'\n';require(snapshot.revision>revision&&snapshot.brightness==before);std::cout<<"Same-value WMI brightness write/read passed\n";}
    }
    OleUninitialize();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<" at "<<checks<<'\n';OleUninitialize();return 1;}}
