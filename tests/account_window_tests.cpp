#include "ui/account_window.h"
#include "ui/settings_window.h"
#include <iostream>

// Capture only windows owned by this test process, without desktop input injection.
bool capture(HWND window,const std::filesystem::path& path){
    RECT rect{};GetWindowRect(window,&rect);int width=rect.right-rect.left,height=rect.bottom-rect.top;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap){DeleteDC(dc);return false;}
    auto previous=SelectObject(dc,bitmap);BOOL printed=PrintWindow(window,dc,2);GdiFlush();
    if(printed){BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof header+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+DWORD(width*height*4);std::ofstream stream(path,std::ios::binary|std::ios::trunc);stream.write(reinterpret_cast<char*>(&header),sizeof header);stream.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));stream.write(static_cast<char*>(pixels),width*height*4);printed=bool(stream);}
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);return printed!=FALSE;
}
int main(){
    OleInitialize(nullptr);int result=0;
    try{
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"Floatlet.AccountUITest";RegisterClassW(&cls);
        auto owner=CreateWindowExW(WS_EX_TOPMOST,cls.lpszClassName,L"Test owner",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);
        delight::Settings settings;
        {int lastAction=0;delight::SettingsWindow ui(owner,settings,[&](int action){lastAction=action;});ui.show();UpdateWindow(ui.native());
            if(GetWindow(ui.native(),GW_OWNER)||GetWindowLongPtrW(ui.native(),GWL_EXSTYLE)&WS_EX_TOPMOST)throw std::runtime_error("Settings inherited island stacking");
            if(!delight::InterfaceFonts::get().collection)throw std::runtime_error("Embedded fonts unavailable");
            if(!capture(ui.native(),L"settings-review.bmp"))throw std::runtime_error("Settings capture failed");
            ui.showSearch();UpdateWindow(ui.native());if(!capture(ui.native(),L"search-settings-review.bmp"))throw std::runtime_error("Search settings capture failed");
            for(int i=0;i<5;++i)SendMessageW(ui.native(),WM_KEYDOWN,VK_TAB,0);
            SendMessageW(ui.native(),WM_KEYDOWN,VK_RETURN,0);if(lastAction!=delight::SettingsWindow::SearchOptions)throw std::runtime_error("Shortcut keyboard action failed");
            SetWindowPos(ui.native(),nullptr,0,0,800,500,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);UpdateWindow(ui.native());
            SendMessageW(ui.native(),WM_MOUSEWHEEL,MAKEWPARAM(0,short(-WHEEL_DELTA*8)),0);UpdateWindow(ui.native());
            if(!capture(ui.native(),L"settings-compact-review.bmp"))throw std::runtime_error("Compact settings capture failed");
        }
        DestroyWindow(owner);
        auto folder=std::filesystem::temp_directory_path()/L"Floatlet-Account-Review";
        delight::AccountWindow ui({L"example.supabase.co",L"sb_publishable_review",true},folder);
        SetTimer(nullptr,0,350,[](HWND,UINT,UINT_PTR timer,DWORD){
            auto window=FindWindowW(L"Floatlet.Account",nullptr);DWORD process=0;if(window)GetWindowThreadProcessId(window,&process);
            if(process!=GetCurrentProcessId())return;KillTimer(nullptr,timer);
            capture(window,L"account-review.bmp");PostMessageW(window,WM_CLOSE,0,0);
        });
        ui.run();
        if(!std::filesystem::exists(L"account-review.bmp"))throw std::runtime_error("Account capture failed");
        std::cout<<"Native settings and account windows rendered and closed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";result=1;}catch(...){std::cerr<<"Native window review failed.\n";result=1;}
    OleUninitialize();return result;
}
