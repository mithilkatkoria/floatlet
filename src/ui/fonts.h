#pragma once
#include <windows.h>
#include <dwrite_3.h>
#include <winrt/base.h>
#include <vector>

namespace delight {
// Open-source fonts live in the executable. No font installation or network request.
struct InterfaceFonts {
    winrt::com_ptr<IDWriteFactory5> factory;
    winrt::com_ptr<IDWriteInMemoryFontFileLoader> loader;
    winrt::com_ptr<IDWriteFontCollection1> collection;
    std::vector<HANDLE> gdi;
    bool registered=false;
    InterfaceFonts(){
        try{
            winrt::com_ptr<IDWriteFactory> base;
            winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(base.put())));
            factory=base.as<IDWriteFactory5>();
            winrt::check_hresult(factory->CreateInMemoryFontFileLoader(loader.put()));
            winrt::check_hresult(factory->RegisterFontFileLoader(loader.get()));registered=true;
            winrt::com_ptr<IDWriteFontSetBuilder1> builder;
            winrt::check_hresult(factory->CreateFontSetBuilder(builder.put()));
            for(int id:{301,302}){
                auto module=GetModuleHandleW(nullptr);auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
                if(!resource)continue;
                auto data=LockResource(LoadResource(module,resource));auto size=SizeofResource(module,resource);
                winrt::com_ptr<IDWriteFontFile> file;
                winrt::check_hresult(loader->CreateInMemoryFontFileReference(factory.get(),data,size,nullptr,file.put()));
                winrt::check_hresult(builder->AddFontFile(file.get()));
                DWORD count=0;if(auto handle=AddFontMemResourceEx(data,size,nullptr,&count))gdi.push_back(handle);
            }
            winrt::com_ptr<IDWriteFontSet> set;winrt::check_hresult(builder->CreateFontSet(set.put()));
            if(set->GetFontCount())winrt::check_hresult(factory->CreateFontCollectionFromFontSet(set.get(),collection.put()));
        }catch(...){collection=nullptr;}
    }
    ~InterfaceFonts(){collection=nullptr;if(registered)factory->UnregisterFontFileLoader(loader.get());for(auto handle:gdi)RemoveFontMemResourceEx(handle);}
    static InterfaceFonts& get(){static InterfaceFonts fonts;return fonts;}
};
}
