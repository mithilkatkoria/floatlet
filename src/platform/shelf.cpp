#include "shelf.h"
#include <shellapi.h>
#include <shlobj.h>
#include <winrt/base.h>
namespace delight {
HRESULT DropTarget::QueryInterface(REFIID id,void** out) {if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDropTarget){*out=static_cast<IDropTarget*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
HRESULT DropTarget::DragEnter(IDataObject* data,DWORD,POINTL,DWORD* effect) {
    FORMATETC fmt{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
    acceptable=(*effect&DROPEFFECT_COPY) && data->QueryGetData(&fmt)==S_OK;
    *effect=acceptable?DROPEFFECT_COPY:DROPEFFECT_NONE; hovering(acceptable); return S_OK;
}
HRESULT DropTarget::DragOver(DWORD,POINTL,DWORD* effect) {*effect=acceptable&&(*effect&DROPEFFECT_COPY)?DROPEFFECT_COPY:DROPEFFECT_NONE;return S_OK;}
HRESULT DropTarget::DragLeave(){hovering(false);acceptable=false;return S_OK;}
HRESULT DropTarget::Drop(IDataObject* data,DWORD,POINTL,DWORD* effect) {
    bool copy=acceptable&&(*effect&DROPEFFECT_COPY); *effect=DROPEFFECT_NONE;
    FORMATETC fmt{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; STGMEDIUM medium{};
    if(copy && SUCCEEDED(data->GetData(&fmt,&medium))) {
        std::vector<std::wstring> paths;
        const auto bytes=medium.tymed==TYMED_HGLOBAL?GlobalSize(medium.hGlobal):0;
        if(bytes>=sizeof(DROPFILES)&&bytes<=8*1024*1024) {
            // Bound all reads of untrusted CF_HDROP memory, including terminators.
            auto raw=static_cast<const unsigned char*>(GlobalLock(medium.hGlobal));
            if(raw){
                DROPFILES header{};memcpy(&header,raw,sizeof(header));
                if(header.fWide&&header.pFiles>=sizeof(DROPFILES)&&header.pFiles%alignof(wchar_t)==0&&header.pFiles<bytes){
                    auto begin=reinterpret_cast<const wchar_t*>(raw+header.pFiles);auto end=begin+(bytes-header.pFiles)/sizeof(wchar_t);
                    bool complete=false;
                    while(begin<end){if(!*begin){complete=true;break;}auto term=std::find(begin,end,L'\0');if(term==end||term-begin>32760||paths.size()>=100)break;paths.emplace_back(begin,term);begin=term+1;}
                    if(!complete)paths.clear();
                }
                GlobalUnlock(medium.hGlobal);
            }
        }
        ReleaseStgMedium(&medium);
        if(!paths.empty()) {accepted(std::move(paths));*effect=DROPEFFECT_COPY;} else rejected();
    } else rejected();
    hovering(false); acceptable=false;return S_OK;
}
class Source final:public IDropSource {
    ULONG refs=1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDropSource){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape,DWORD keys) override {return escape?DRAGDROP_S_CANCEL:!(keys&MK_LBUTTON)?DRAGDROP_S_DROP:S_OK;}
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override{return DRAGDROP_S_USEDEFAULTCURSORS;}
};
class FileData final:public IDataObject {
    ULONG refs=1;std::wstring path;
public:
    explicit FileData(std::wstring p):path(std::move(p)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDataObject)return E_NOINTERFACE;*out=static_cast<IDataObject*>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f) override{if(!f)return E_POINTER;if(f->cfFormat!=CF_HDROP)return DV_E_FORMATETC;if(!(f->tymed&TYMED_HGLOBAL))return DV_E_TYMED;if(f->dwAspect!=DVASPECT_CONTENT)return DV_E_DVASPECT;if(f->lindex!=-1)return DV_E_LINDEX;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f,STGMEDIUM* medium) override{
        if(!medium)return E_POINTER;*medium={};auto hr=QueryGetData(f);if(FAILED(hr))return hr;
        auto bytes=sizeof(DROPFILES)+(path.size()+2)*sizeof(wchar_t);HGLOBAL block=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!block)return E_OUTOFMEMORY;
        auto data=static_cast<BYTE*>(GlobalLock(block));if(!data){GlobalFree(block);return E_OUTOFMEMORY;}DROPFILES header{};header.pFiles=sizeof(DROPFILES);header.fWide=TRUE;memcpy(data,&header,sizeof(header));memcpy(data+sizeof(header),path.c_str(),(path.size()+1)*sizeof(wchar_t));GlobalUnlock(block);medium->tymed=TYMED_HGLOBAL;medium->hGlobal=block;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*,STGMEDIUM*) override{return DATA_E_FORMATETC;}
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,FORMATETC* out) override{if(!out)return E_POINTER;out->ptd=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*,STGMEDIUM*,BOOL) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction,IEnumFORMATETC** out) override{if(direction!=DATADIR_GET)return E_NOTIMPL;FORMATETC f{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};return SHCreateStdEnumFmtEtc(1,&f,out);}
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*,DWORD,IAdviseSink*,DWORD*) override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override{return OLE_E_ADVISENOTSUPPORTED;}
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override{return OLE_E_ADVISENOTSUPPORTED;}
};
HRESULT createFileData(const std::wstring& path,IDataObject** out){if(!out)return E_POINTER;*out=nullptr;if(path.empty()||path.size()>32760||path.find(L'\0')!=std::wstring::npos)return E_INVALIDARG;*out=new FileData(path);return S_OK;}
HRESULT dragCopy(HWND owner,const std::wstring& path) {
    winrt::com_ptr<IDataObject> data;auto hr=createFileData(path,data.put());if(FAILED(hr))return hr;
    auto source=new Source;DWORD effect=0;hr=DoDragDrop(data.get(),source,DROPEFFECT_COPY,&effect);source->Release();(void)owner;return hr;
}
std::vector<std::wstring> pickFiles(HWND owner) {
    winrt::com_ptr<IFileOpenDialog> dialog; winrt::check_hresult(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.put())));
    dialog->SetOptions(FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST|FOS_NOCHANGEDIR);
    std::vector<std::wstring> result; if(FAILED(dialog->Show(owner)))return result;
    winrt::com_ptr<IShellItemArray> items; winrt::check_hresult(dialog->GetResults(items.put()));DWORD count=0;items->GetCount(&count);
    if(count>100)throw std::runtime_error("Select at most 100 files");
    for(DWORD i=0;i<count;++i) {winrt::com_ptr<IShellItem> item;items->GetItemAt(i,item.put());PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result.emplace_back(path);CoTaskMemFree(path);}}
    return result;
}
}
