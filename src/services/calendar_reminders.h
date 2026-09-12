#pragma once
#include "ical.h"
namespace delight {
struct CalendarReminders {
    std::map<std::pair<std::string,ical::sys_seconds>,ical::sys_seconds> shown;
    std::optional<ical::Entry> next(const std::vector<ical::Entry>& entries,ical::sys_seconds now){
        std::erase_if(shown,[&](const auto& item){return item.second<=now;});
        for(const auto& entry:entries){
            auto left=entry.start-now;
            auto key=std::make_pair(entry.uid.empty()?entry.title:entry.uid,entry.start);
            if(!entry.allDay&&left>std::chrono::seconds{0}&&left<=std::chrono::minutes{5}&&!shown.contains(key)){
                if(shown.size()>=256)shown.erase(shown.begin());
                shown.emplace(key,entry.start);return entry;
            }
        }
        return std::nullopt;
    }
};
}
