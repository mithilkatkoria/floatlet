#pragma once
#include "options.h"
#include <windows.h>
#include <string>
namespace delight::search {
// Two bits per modifier: left, right. Zero retains the legacy either-side chord.
inline unsigned heldSides(){unsigned s=0;unsigned keys[]={VK_LMENU,VK_RMENU,VK_LCONTROL,VK_RCONTROL,VK_LSHIFT,VK_RSHIFT};for(unsigned i=0;i<6;++i)if(GetAsyncKeyState(keys[i])&0x8000)s|=1u<<i;return s;}
inline unsigned sideModifiers(unsigned s){return ((s&3)?MOD_ALT:0)|((s&12)?MOD_CONTROL:0)|((s&48)?MOD_SHIFT:0);}
inline unsigned normalizeSides(unsigned s){if(s&2)s&=~4u;return s;} // AltGr's synthetic left Control.
inline bool validShortcut(const Options& o){return o.key>0&&o.key<256&&o.key!=VK_F12&&!(o.key==VK_F4&&(o.modifiers&MOD_ALT))&&o.key!=VK_TAB&&o.key!=VK_ESCAPE&&o.key!=VK_DELETE&&o.key!=VK_LWIN&&o.key!=VK_RWIN&&!(o.key>=VK_SHIFT&&o.key<=VK_MENU)&&!(o.key>=VK_LSHIFT&&o.key<=VK_RMENU)&&o.modifiers>0&&o.modifiers<=7&&o.sides<=63&&(!o.sides||sideModifiers(o.sides)==o.modifiers);}
inline bool matchesShortcut(const Options& o,unsigned sides){return o.sides?normalizeSides(sides)==o.sides:sideModifiers(sides)==o.modifiers;}
inline std::wstring shortcutLabel(const Options& o){std::wstring text;const wchar_t* names[]={L"Left Alt + ",L"Right Alt + ",L"Left Ctrl + ",L"Right Ctrl + ",L"Left Shift + ",L"Right Shift + "};if(o.sides){for(unsigned i=0;i<6;++i)if(o.sides&(1u<<i))text+=names[i];}else{if(o.modifiers&MOD_CONTROL)text+=L"Ctrl + ";if(o.modifiers&MOD_ALT)text+=L"Alt + ";if(o.modifiers&MOD_SHIFT)text+=L"Shift + ";}wchar_t key[64]{};GetKeyNameTextW(LONG((MapVirtualKeyW(o.key,MAPVK_VK_TO_VSC)<<16)|(o.extended?1u<<24:0)),key,64);return text+(key[0]?key:L"Key");}
class Shortcut {
    HWND owner{};HHOOK hook{};Options options;bool down=false;inline static Shortcut* active=nullptr;
    static LRESULT CALLBACK keyboard(int code,WPARAM w,LPARAM l){auto p=active;if(code==HC_ACTION&&p){auto k=reinterpret_cast<KBDLLHOOKSTRUCT*>(l);if(k->vkCode==p->options.key){bool up=w==WM_KEYUP||w==WM_SYSKEYUP;if(up&&p->down){p->down=false;return 1;}if(!up&&(p->down||(!(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000)&&matchesShortcut(p->options,heldSides())&&bool(k->flags&LLKHF_EXTENDED)==p->options.extended))){if(!p->down){p->down=true;PostMessageW(p->owner,WM_HOTKEY,3,0);}return 1;}}}return CallNextHookEx(nullptr,code,w,l);}
public:
    ~Shortcut(){reset();}
    void reset(){if(hook){UnhookWindowsHookEx(hook);hook=nullptr;}if(active==this)active=nullptr;if(owner)UnregisterHotKey(owner,3);owner=nullptr;down=false;}
    bool configure(HWND hwnd,const Options& o){reset();if(!o.enabled||!validShortcut(o))return false;owner=hwnd;options=o;if(!o.sides)return RegisterHotKey(hwnd,3,o.modifiers|MOD_NOREPEAT,o.key)!=FALSE;active=this;hook=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,GetModuleHandleW(nullptr),0);if(!hook)active=nullptr;return hook!=nullptr;}
};
}
