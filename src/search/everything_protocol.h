#pragma once
#include "model.h"
#include <cstring>
#include <optional>

namespace delight::search {
inline constexpr size_t EverythingReplyLimit=2*1024*1024;

// Decode the documented, packed Unicode v1 reply without trusting offsets or counts.
inline std::optional<std::vector<Result>> decodeEverythingReply(const void* data,size_t size){
    if(!data||size<28||size>EverythingReplyLimit)return std::nullopt;
    auto bytes=static_cast<const unsigned char*>(data);
    auto word=[&](size_t offset){std::uint32_t value;std::memcpy(&value,bytes+offset,4);return value;};
    auto count=word(20);
    if(count>512||28+size_t(count)*12>size)return std::nullopt;
    size_t copied=0;
    auto string=[&](std::uint32_t offset,std::wstring& value){
        if(offset<28+size_t(count)*12||offset%2||offset>=size)return false;
        for(size_t pos=offset;pos+2<=size&&value.size()<32760;pos+=2){
            std::uint16_t c;std::memcpy(&c,bytes+pos,2);
            if(!c)return true;
            // Repeated offsets must not amplify a small packet into unbounded allocations.
            if(++copied>EverythingReplyLimit/2)return false;
            value+=static_cast<wchar_t>(c);
        }
        return false;
    };
    std::vector<Result> results;
    results.reserve(count);
    for(std::uint32_t i=0;i<count;++i){
        auto at=28+size_t(i)*12;Result r;std::wstring path;
        if(!string(word(at+4),r.title)||!string(word(at+8),path))return std::nullopt;
        r.kind=(word(at)&1)?Kind::Folder:Kind::File;
        r.detail=path;r.target=path;
        if(!r.target.empty()&&r.target.back()!=L'\\')r.target+=L'\\';
        r.target+=r.title;r.id=L"path:"+fold(r.target);
        results.push_back(std::move(r));
    }
    return results;
}
}
