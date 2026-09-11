#pragma once
#include <windows.h>
#include <oleidl.h>
#include <functional>
#include <string>
#include <vector>
namespace delight {
class DropTarget final:public IDropTarget {
    ULONG refs=1;
    bool acceptable=false;
public:
    std::function<void(bool)> hovering;
    std::function<void(std::vector<std::wstring>)> accepted;
    std::function<void()> rejected;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override;
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*,DWORD,POINTL,DWORD*) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD*) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject*,DWORD,POINTL,DWORD*) override;
};
HRESULT createFileData(const std::wstring& path,IDataObject** out);
HRESULT dragCopy(HWND owner,const std::wstring& path);
std::vector<std::wstring> pickFiles(HWND owner);
}
