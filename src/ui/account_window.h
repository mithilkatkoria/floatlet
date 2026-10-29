#pragma once
#include "services/account.h"
#include "search/paint.h"
#include "ui/fonts.h"
#include <dwmapi.h>
#include <mutex>
#include <thread>

namespace delight {
class AccountWindow {
    enum {Primary=101,Google,Switch,Recovery,Privacy,Terms,Email=201,Password,Confirm,Finished=WM_APP+201};
    account::Config config;account::Store store;search::Paint paint;
    HWND window{},email{},password{},confirm{},emailLabel{},passwordLabel{},confirmLabel{},primary{},google{},switchMode{},recovery{},privacy{},terms{};
    HFONT font{},smallFont{};HBRUSH fieldBrush{},backgroundBrush{};std::thread worker;std::atomic<bool> cancel=false;std::atomic<HWND> destination{};
    std::mutex resultMutex;std::optional<account::Session> result;std::wstring resultMessage,status;
    bool creating=true,busy=false,accepted=false,confirmation=false;std::wstring signedEmail;
    float scale()const{return window?GetDpiForWindow(window)/96.f:1.f;}
    static std::wstring value(HWND field){int length=GetWindowTextLengthW(field);std::wstring text(size_t(length)+1,L'\0');GetWindowTextW(field,text.data(),length+1);text.resize(length);return text;}
    void refresh(){if(window)InvalidateRect(window,nullptr,FALSE);}
    void enable(bool enabled){busy=!enabled;for(auto child:{email,password,confirm,primary,google,switchMode,recovery})EnableWindow(child,enabled);refresh();}
    void layout(){
        RECT rect{};GetClientRect(window,&rect);float s=scale(),width=rect.right/s,x=width*.43f,form=width-x-38;
        auto move=[&](HWND h,float y,float height){SetWindowPos(h,nullptr,int(x*s),int(y*s),int(form*s),int(height*s),SWP_NOZORDER|SWP_NOACTIVATE);};
        move(google,139,43);ShowWindow(google,config.googleEnabled?SW_SHOW:SW_HIDE);move(emailLabel,214,20);move(email,247,23);move(passwordLabel,293,20);move(password,326,23);
        move(confirmLabel,372,20);move(confirm,405,23);ShowWindow(confirmLabel,creating?SW_SHOW:SW_HIDE);ShowWindow(confirm,creating?SW_SHOW:SW_HIDE);
        move(primary,creating?462.f:389.f,44);move(switchMode,creating?514.f:441.f,31);move(recovery,480,28);ShowWindow(recovery,creating?SW_HIDE:SW_SHOW);
        SetWindowPos(privacy,nullptr,int(28*s),int(563*s),int((x-70)*.62f*s),int(31*s),SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowPos(terms,nullptr,int((28+(x-70)*.62f+8)*s),int(563*s),int((x-70)*.38f*s),int(31*s),SWP_NOZORDER|SWP_NOACTIVATE);
        SetWindowTextW(primary,creating?L"Create free account":L"Sign in to Floatlet");SetWindowTextW(switchMode,creating?L"Already have an account? Sign in":L"New to Floatlet? Create an account");refresh();
    }
    void launch(int action){
        if(busy)return;if(action==Google&&!config.googleEnabled)return;
        if(action==Switch){creating=!creating;status.clear();SetWindowTextW(password,L"");SetWindowTextW(confirm,L"");layout();return;}
        if(action==Privacy||action==Terms||action==Recovery){ShellExecuteW(window,L"open",action==Privacy?L"https://floatlet.vercel.app/privacy/":action==Terms?L"https://floatlet.vercel.app/terms/":L"https://floatlet.vercel.app/account/",nullptr,nullptr,SW_SHOWNORMAL);return;}
        auto address=value(email),secret=value(password);
        if(action==Primary){
            auto at=address.find(L'@');if(at==std::wstring::npos||at==0||address.find(L'.',at)==std::wstring::npos||address.size()>320){status=L"Enter a valid email address.";SetFocus(email);refresh();return;}
            if(secret.empty()){status=L"Enter your password.";SetFocus(password);refresh();return;}
            if(creating){
                size_t characters=0;for(wchar_t c:secret)if(c<0xdc00||c>0xdfff)++characters;
                if(characters<15||secret.find_first_not_of(secret.front())==std::wstring::npos){status=L"Use at least 15 characters. A long passphrase works well.";SetFocus(password);refresh();return;}
                if(secret!=value(confirm)){status=L"Those passwords do not match.";SetFocus(confirm);refresh();return;}
            }
        }
        start(action,std::move(address),std::move(secret));
    }
    void start(int action,std::wstring address={},std::wstring secret={}){
        if(worker.joinable())worker.join();enable(false);cancel=false;
        status=action==Google?L"Complete Google sign-in in your browser.":action==0?L"Checking your account…":creating?L"Creating your account…":L"Signing you in…";refresh();
        bool create=creating;auto previous=action==0?store.load():std::optional<account::Session>{};
        worker=std::thread([this,action,create,address=std::move(address),secret=std::move(secret),previous]()mutable{
            std::optional<account::Session> session;std::wstring message;bool needsConfirmation=false;
            try{
                winrt::init_apartment();struct Apartment{~Apartment(){winrt::uninit_apartment();}}apartment;account::Client client(config);
                if(action==0){if(!previous)throw account::Error("Sign in to continue.");try{session=client.refresh(*previous);}catch(const account::Error& error){if(error.network&&previous->usable())session=previous;else throw;}}
                else if(action==Google)session=client.google(cancel);
                else if(create){session=client.signUp(address,secret);needsConfirmation=!session;message=L"Check your email to confirm your account, then sign in.";}
                else session=client.signIn(address,secret);
            }catch(const std::exception& error){message=winrt::to_hstring(error.what());}catch(...){message=L"Sign-in could not be completed. Please try again.";}
            if(!secret.empty())SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));
            {std::scoped_lock lock(resultMutex);result=std::move(session);resultMessage=std::move(message);confirmation=needsConfirmation;}
            if(!cancel)if(auto target=destination.load())PostMessageW(target,Finished,0,0);
        });
        SetWindowTextW(password,L"");SetWindowTextW(confirm,L"");
    }
    void finish(){
        if(worker.joinable())worker.join();std::optional<account::Session> session;
        {std::scoped_lock lock(resultMutex);session=std::move(result);status=std::move(resultMessage);if(confirmation)creating=false;}
        if(session){try{store.save(*session);signedEmail=session->email;accepted=true;DestroyWindow(window);return;}catch(const std::exception& error){status=winrt::to_hstring(error.what());}}
        enable(true);layout();
    }
    void draw(HDC dc){
        RECT bounds{};GetClientRect(window,&bounds);float s=scale(),width=bounds.right/s,height=bounds.bottom/s,x=width*.43f,form=width-x-38;
        try{paint.begin(dc,bounds);paint.clear(RGB(255,255,254));
            paint.round(0,0,(x-27)*s,height*s,0,RGB(245,244,240));
            paint.text(L"floatlet",29*s,29*s,(x-60)*s,40*s,26*s,RGB(37,36,34),true);
            paint.text(L"YOUR DESKTOP COMPANION",29*s,90*s,(x-60)*s,22*s,9*s,RGB(117,114,108),true,true);
            paint.text(L"Your desktop.",29*s,194*s,(x-58)*s,63*s,40*s,RGB(37,36,34),false,false,true);
            paint.text(L"Within reach.",29*s,247*s,(x-58)*s,63*s,40*s,RGB(37,36,34),false,false,true);
            paint.text(L"Music, files, timers and search.",29*s,335*s,(x-57)*s,27*s,14*s,RGB(117,114,108));
            paint.text(L"A little closer to what you need.",29*s,362*s,(x-57)*s,27*s,14*s,RGB(117,114,108));
            paint.round(29*s,426*s,(x-86)*s,52*s,26*s,RGB(5,8,14));paint.round(37*s,434*s,36*s,36*s,10*s,RGB(220,195,151));
            paint.text(L"♪",48*s,436*s,27*s,29*s,22*s,RGB(28,23,42),true);
            paint.text(L"Your little island",85*s,437*s,(x-150)*s,22*s,12*s,RGB(241,242,247),true);
            paint.text(L"Free and open source",85*s,456*s,(x-150)*s,19*s,9*s,RGB(152,164,183));
            paint.text(L"Creating an account means you agree",29*s,515*s,(x-60)*s,21*s,10*s,RGB(117,114,108));
            paint.text(L"to the Terms and Privacy notice.",29*s,535*s,(x-60)*s,21*s,10*s,RGB(117,114,108));
            paint.text(L"WELCOME TO FLOATLET",x*s,33*s,form*s,21*s,10*s,RGB(117,114,108),true,true);
            paint.text(creating?L"Create your account.":L"Welcome back.",x*s,66*s,form*s,44*s,29*s,RGB(37,36,34),true);
            paint.text(L"A free account. No subscription.",x*s,108*s,form*s,23*s,13*s,RGB(117,114,108));
            paint.text(config.googleEnabled?L"OR WITH EMAIL":L"CONTINUE WITH EMAIL",x*s,194*s,form*s,20*s,9*s,RGB(117,114,108),true,true);
            for(float y:{235.f,314.f}){paint.round((x-1)*s,y*s,(form+2)*s,47*s,10*s,RGB(249,248,245));paint.outline((x-1)*s,y*s,(form+2)*s,47*s,10*s,RGB(222,219,212));}
            if(creating){paint.round((x-1)*s,393*s,(form+2)*s,47*s,10*s,RGB(249,248,245));paint.outline((x-1)*s,393*s,(form+2)*s,47*s,10*s,RGB(222,219,212));}
            auto split=status.size()>67?status.rfind(L' ',67):std::wstring::npos;
            paint.text(split==std::wstring::npos?status:status.substr(0,split),x*s,(creating?554:528)*s,form*s,22*s,11*s,RGB(100,78,137));
            if(split!=std::wstring::npos)paint.text(status.substr(split+1),x*s,(creating?576:550)*s,form*s,22*s,11*s,RGB(100,78,137));
            paint.end();
        }catch(...){}
    }
    void drawButton(const DRAWITEMSTRUCT& item){
        auto rect=item.rcItem;auto id=item.CtlID;bool link=id==Switch||id==Recovery||id==Privacy||id==Terms;COLORREF bg=id==Primary?RGB(37,36,34):RGB(255,255,254),fg=id==Primary?RGB(255,255,254):RGB(62,58,52);
        if(id==Privacy||id==Terms){HBRUSH surface=CreateSolidBrush(RGB(245,244,240));FillRect(item.hDC,&rect,surface);DeleteObject(surface);}else FillRect(item.hDC,&rect,backgroundBrush);
        if(id==Privacy||id==Terms)bg=RGB(245,244,240);if(item.itemState&ODS_SELECTED)bg=id==Primary?RGB(65,61,55):RGB(238,235,228);
        HBRUSH brush=CreateSolidBrush(bg);HPEN pen=CreatePen(PS_SOLID,1,item.itemState&ODS_FOCUS?RGB(111,89,158):link?bg:RGB(222,219,212));auto oldBrush=SelectObject(item.hDC,brush),oldPen=SelectObject(item.hDC,pen);RoundRect(item.hDC,rect.left,rect.top,rect.right,rect.bottom,12,12);SelectObject(item.hDC,oldBrush);SelectObject(item.hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
        SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,item.itemState&ODS_DISABLED?RGB(160,155,145):fg);auto oldFont=SelectObject(item.hDC,link?smallFont:font);auto text=value(item.hwndItem);DrawTextW(item.hDC,text.c_str(),-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(item.hDC,oldFont);
    }
    void create(){
        paint.typography(InterfaceFonts::get().collection.get());float s=scale();font=CreateFontW(-int(14*s),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Figtree");smallFont=CreateFontW(-int(11*s),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Figtree");fieldBrush=CreateSolidBrush(RGB(249,248,245));backgroundBrush=CreateSolidBrush(RGB(255,255,254));
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return child;};
        google=control(L"BUTTON",L"Continue with Google",BS_OWNERDRAW|WS_TABSTOP,Google);
        emailLabel=control(L"STATIC",L"Email address",0,0);email=control(L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,Email);SendMessageW(email,EM_LIMITTEXT,320,0);
        passwordLabel=control(L"STATIC",L"Password",0,0);password=control(L"EDIT",L"",ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP,Password);SendMessageW(password,EM_LIMITTEXT,256,0);
        confirmLabel=control(L"STATIC",L"Confirm password",0,0);confirm=control(L"EDIT",L"",ES_PASSWORD|ES_AUTOHSCROLL|WS_TABSTOP,Confirm);SendMessageW(confirm,EM_LIMITTEXT,256,0);
        for(auto field:{email,password,confirm})SendMessageW(field,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(int(12*s),int(12*s)));
        primary=control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,Primary);switchMode=control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,Switch);recovery=control(L"BUTTON",L"Forgot password? Open account help",BS_OWNERDRAW|WS_TABSTOP,Recovery);privacy=control(L"BUTTON",L"Privacy notice ↗",BS_OWNERDRAW|WS_TABSTOP,Privacy);
        terms=control(L"BUTTON",L"Terms ↗",BS_OWNERDRAW|WS_TABSTOP,Terms);
        layout();SetFocus(email);
    }
    static LRESULT CALLBACK proc(HWND h,UINT message,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<AccountWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(message==WM_NCCREATE){self=static_cast<AccountWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window=h;self->destination=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}if(!self)return DefWindowProcW(h,message,w,l);
        switch(message){
        case WM_CREATE:self->create();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT paint{};BeginPaint(h,&paint);self->draw(paint.hdc);EndPaint(h,&paint);return 0;}
        case WM_DRAWITEM:self->drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
        case WM_CTLCOLOREDIT:SetTextColor(reinterpret_cast<HDC>(w),RGB(37,36,34));SetBkColor(reinterpret_cast<HDC>(w),RGB(249,248,245));return reinterpret_cast<LRESULT>(self->fieldBrush);
        case WM_CTLCOLORSTATIC:SetTextColor(reinterpret_cast<HDC>(w),RGB(77,73,66));SetBkColor(reinterpret_cast<HDC>(w),RGB(255,255,254));return reinterpret_cast<LRESULT>(self->backgroundBrush);
        case WM_COMMAND:if(HIWORD(w)==BN_CLICKED)self->launch(LOWORD(w));return 0;
        case Finished:self->finish();return 0;
        case WM_CLOSE:self->cancel=true;DestroyWindow(h);return 0;
        case WM_DESTROY:self->destination=nullptr;self->window=nullptr;return 0;
        }return DefWindowProcW(h,message,w,l);
    }
public:
    AccountWindow(account::Config value,std::filesystem::path folder):config(std::move(value)),store(folder/(L"account-"+config.host+L".dat")){}
    ~AccountWindow(){cancel=true;if(worker.joinable())worker.join();if(font)DeleteObject(font);if(smallFont)DeleteObject(smallFont);if(fieldBrush)DeleteObject(fieldBrush);if(backgroundBrush)DeleteObject(backgroundBrush);}
    const std::wstring& emailAddress()const{return signedEmail;}
    bool run(){
        WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=proc;cls.lpszClassName=L"Floatlet.Account";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(101));RegisterClassW(&cls);
        window=CreateWindowExW(WS_EX_APPWINDOW,cls.lpszClassName,L"Welcome to Floatlet",WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,920,660,nullptr,nullptr,cls.hInstance,this);if(!window)return false;
        BOOL dark=FALSE;DwmSetWindowAttribute(window,20,&dark,sizeof dark);DWORD corners=2;DwmSetWindowAttribute(window,33,&corners,sizeof corners);
        POINT pointer{};GetCursorPos(&pointer);MONITORINFO monitor{sizeof monitor};GetMonitorInfoW(MonitorFromPoint(pointer,MONITOR_DEFAULTTONEAREST),&monitor);float s=scale();int width=int(920*s),height=int(660*s);SetWindowPos(window,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER);
        layout();ShowWindow(window,SW_SHOW);SetForegroundWindow(window);if(store.load()){creating=false;layout();start(0);}
        MSG message{};while(window&&GetMessageW(&message,nullptr,0,0)>0){if(message.message==WM_KEYDOWN&&message.wParam==VK_RETURN){launch(Primary);continue;}if(message.message==WM_KEYDOWN&&message.wParam==VK_ESCAPE){SendMessageW(window,WM_CLOSE,0,0);continue;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
        cancel=true;if(worker.joinable())worker.join();return accepted;
    }
};
}
