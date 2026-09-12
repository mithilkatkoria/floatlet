#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <optional>
#include <stdexcept>
namespace delight::ical {
using namespace std::chrono;
struct Stamp {local_seconds local{};std::string zone;bool utc=false,allDay=false;};
inline int number(std::string_view value){if(value.empty()||value.size()>8)throw std::runtime_error("Invalid calendar number");int out=0;bool negative=value[0]=='-';for(size_t i=negative?1:0;i<value.size();++i){if(value[i]<'0'||value[i]>'9')throw std::runtime_error("Invalid calendar number");out=out*10+value[i]-'0';}return negative?-out:out;}
inline std::vector<std::string> split(std::string_view value,char separator){std::vector<std::string> out;size_t start=0;for(size_t i=0;i<=value.size();++i)if(i==value.size()||value[i]==separator){out.emplace_back(value.substr(start,i-start));start=i+1;}return out;}
inline Stamp stamp(std::string_view value,std::string zone={}){
    if(value.size()!=8&&value.size()!=15&&value.size()!=16)throw std::runtime_error("Unsupported calendar date");
    year_month_day date{year{number(value.substr(0,4))},month{unsigned(number(value.substr(4,2)))},day{unsigned(number(value.substr(6,2)))}};if(!date.ok())throw std::runtime_error("Invalid calendar date");
    Stamp s;s.local=local_days{date};s.zone=std::move(zone);s.allDay=value.size()==8;
    if(!s.allDay){if(value[8]!='T')throw std::runtime_error("Invalid calendar time");int h=number(value.substr(9,2)),m=number(value.substr(11,2)),sec=number(value.substr(13,2));if(h>23||m>59||sec>60)throw std::runtime_error("Invalid calendar time");s.local+=hours{h}+minutes{m}+seconds{sec};s.utc=value.size()==16&&value.back()=='Z';if(value.size()==16&&!s.utc)throw std::runtime_error("Unsupported calendar offset");}return s;
}
inline sys_seconds absolute(const Stamp& s){if(s.utc)return sys_seconds{s.local.time_since_epoch()};auto zone=s.zone.empty()?current_zone():locate_zone(s.zone);return zone->to_sys(s.local,choose::earliest);}
inline std::string unescape(std::string_view text){std::string out;out.reserve(std::min<size_t>(text.size(),512));for(size_t i=0;i<text.size();++i){char c=text[i];if(c=='\\'&&i+1<text.size()){c=text[++i];if(c=='n'||c=='N')c=' ';}if(static_cast<unsigned char>(c)>=32)out+=c;}if(out.size()>512){size_t boundary=512;while(boundary>0&&(static_cast<unsigned char>(out[boundary])&0xc0)==0x80)--boundary;out.resize(boundary);}return out;}
struct Event {std::string uid,title,location;Stamp start;std::optional<Stamp> end,recurrence;std::vector<Stamp> exclude,extra;std::map<std::string,std::string> rule;bool cancelled=false;};
struct Calendar {std::vector<Event> events;unsigned unsupported=0;};
inline Calendar parse(const std::string& raw){
    if(raw.size()>2*1024*1024||raw.find("BEGIN:VCALENDAR")==std::string::npos||raw.find("END:VCALENDAR")==std::string::npos)throw std::runtime_error("Invalid or oversized calendar feed");
    std::vector<std::string> lines;std::istringstream input(raw);std::string line;while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.size()>65536)throw std::runtime_error("Calendar line too long");if(!line.empty()&&(line[0]==' '||line[0]=='\t')&&!lines.empty()){if(lines.back().size()+line.size()>65536)throw std::runtime_error("Calendar line too long");lines.back()+=line.substr(1);}else lines.push_back(line);}
    Calendar out;Event event;bool inside=false,hasStart=false,bad=false;int nested=0;
    for(auto& l:lines){if(l=="BEGIN:VEVENT"){inside=true;hasStart=bad=false;nested=0;event={};continue;}if(!inside)continue;if(l=="END:VEVENT"){if(hasStart&&!bad){if(out.events.size()>=5000)throw std::runtime_error("Calendar has too many events");out.events.push_back(std::move(event));}else ++out.unsupported;inside=false;continue;}if(l.starts_with("BEGIN:")){++nested;continue;}if(l.starts_with("END:")){--nested;continue;}if(nested)continue;
        auto colon=l.find(':');if(colon==std::string::npos)continue;auto properties=split(std::string_view(l).substr(0,colon),';');std::string zone;for(size_t i=1;i<properties.size();++i)if(properties[i].starts_with("TZID=")){zone=properties[i].substr(5);if(zone.size()>1&&zone.front()=='"'&&zone.back()=='"')zone=zone.substr(1,zone.size()-2);}
        auto key=properties[0];auto value=std::string_view(l).substr(colon+1);
        try{if(key=="DTSTART"){event.start=stamp(value,zone);hasStart=true;}else if(key=="DTEND")event.end=stamp(value,zone);else if(key=="UID")event.uid=std::string(value.substr(0,512));else if(key=="SUMMARY")event.title=unescape(value);else if(key=="LOCATION")event.location=unescape(value);else if(key=="STATUS")event.cancelled=value=="CANCELLED";else if(key=="RECURRENCE-ID")event.recurrence=stamp(value,zone);else if(key=="EXDATE"||key=="RDATE"){for(auto& entry:split(value,',')){auto& target=key=="EXDATE"?event.exclude:event.extra;if(target.size()<1000)target.push_back(stamp(entry,zone));}}else if(key=="RRULE"){for(auto& field:split(value,';')){auto equal=field.find('=');if(equal!=std::string::npos)event.rule[field.substr(0,equal)]=field.substr(equal+1);}}else if(key=="DURATION"){bad=true;}}
        catch(...){bad=true;}
    }return out;
}
inline bool listContains(const std::string& text,int value){for(auto& item:split(text,','))if(number(item)==value)return true;return false;}
inline bool matches(const Event& e,local_days candidate){
    auto base=floor<days>(e.start.local);if(candidate<base)return false;if(e.rule.empty())return candidate==base;
    auto get=[&](const char* k){auto i=e.rule.find(k);return i==e.rule.end()?std::string{}:i->second;};auto frequency=get("FREQ");int interval=get("INTERVAL").empty()?1:number(get("INTERVAL"));if(interval<1||interval>3660)throw std::runtime_error("Unsupported recurrence interval");
    year_month_day b{base},d{candidate};int distance=int((candidate-base).count()),monthDistance=(int(d.year())-int(b.year()))*12+int(unsigned(d.month()))-int(unsigned(b.month()));
    if(frequency=="DAILY"){if(distance%interval)return false;}
    else if(frequency=="WEEKLY"){auto startWeek=base-days{weekday{base}.iso_encoding()-1};if(((candidate-startWeek).count()/7)%interval)return false;if(get("BYDAY").empty()&&weekday{candidate}!=weekday{base})return false;}
    else if(frequency=="MONTHLY"){if(monthDistance%interval)return false;if(get("BYMONTHDAY").empty()&&get("BYDAY").empty()&&d.day()!=b.day())return false;}
    else if(frequency=="YEARLY"){if((int(d.year())-int(b.year()))%interval)return false;if(get("BYMONTH").empty()&&d.month()!=b.month())return false;if(get("BYMONTHDAY").empty()&&get("BYDAY").empty()&&d.day()!=b.day())return false;}
    else throw std::runtime_error("Unsupported recurrence frequency");
    if(!get("BYMONTH").empty()&&!listContains(get("BYMONTH"),int(unsigned(d.month()))))return false;
    auto monthEnd=unsigned(year_month_day_last{d.year(),month_day_last{d.month()}}.day());int day=int(unsigned(d.day()));
    if(!get("BYMONTHDAY").empty()&&!listContains(get("BYMONTHDAY"),day)&&!listContains(get("BYMONTHDAY"),day-int(monthEnd)-1))return false;
    if(!get("BYDAY").empty()){const char* names[]={"MO","TU","WE","TH","FR","SA","SU"};std::string name=names[weekday{candidate}.iso_encoding()-1];bool found=false;for(auto& token:split(get("BYDAY"),',')){if(token.size()<2||token.substr(token.size()-2)!=name)continue;if(token.size()==2){found=true;break;}int ordinal=number(std::string_view(token).substr(0,token.size()-2));if((ordinal>0&&(day-1)/7+1==ordinal)||(ordinal<0&&-int((monthEnd-day)/7+1)==ordinal)){found=true;break;}}if(!found)return false;}
    return true;
}
struct Entry {std::string title,location;int minute=0,endMinute=0;bool allDay=false;std::string uid;sys_seconds start{};};
struct Agenda {std::vector<Entry> entries;unsigned unsupported=0;};
inline Agenda day(const Calendar& calendar,year_month_day selected){
    unsigned expansionSteps=0;Agenda out;out.unsupported=calendar.unsupported;auto zone=current_zone();local_days selectedDay{selected};auto begin=zone->to_sys(local_seconds{selectedDay},choose::earliest),end=zone->to_sys(local_seconds{selectedDay+days{1}},choose::earliest);
    std::set<std::pair<std::string,sys_seconds>> overrides;for(auto& e:calendar.events)if(e.recurrence)try{overrides.emplace(e.uid,absolute(*e.recurrence));}catch(...){++out.unsupported;}
    for(auto& e:calendar.events){try{
        if(e.cancelled)continue;for(auto& [key,value]:e.rule)if(key!="FREQ"&&key!="INTERVAL"&&key!="COUNT"&&key!="UNTIL"&&key!="BYDAY"&&key!="BYMONTHDAY"&&key!="BYMONTH"&&!(key=="WKST"&&value=="MO"))throw std::runtime_error("Unsupported recurrence rule");
        if(e.end&&(e.start.utc!=e.end->utc||e.start.zone!=e.end->zone))throw std::runtime_error("Mixed event timezones are unsupported");
        auto duration=e.end?e.end->local-e.start.local:e.start.allDay?seconds{86400}:seconds{1};if(duration<seconds{0}||duration>days{366})throw std::runtime_error("Unsupported event duration");
        auto emit=[&](Stamp start){auto at=absolute(start);Stamp finish=start;finish.local+=duration;auto until=absolute(finish);if(at>=end||until<=begin)return;if(!e.recurrence&&overrides.contains({e.uid,at}))return;for(auto& exclude:e.exclude)if(absolute(exclude)==at)return;
            if(out.entries.size()>=100)return;auto local=zone->to_local(at),localEnd=zone->to_local(until);int minute=int(duration_cast<minutes>(local-local_seconds{selectedDay}).count()),endMinute=int(duration_cast<minutes>(localEnd-local_seconds{selectedDay}).count());out.entries.push_back({e.title.empty()?"Untitled event":e.title,e.location,minute,endMinute,start.allDay,e.uid,at});};
        if(e.rule.empty()||e.recurrence)emit(e.start);
        else {auto base=floor<days>(e.start.local);auto offset=e.start.local-base;auto first=selectedDay-days{2}-ceil<days>(duration);auto expansionEnd=selectedDay+days{2};auto countIt=e.rule.find("COUNT");int remaining=countIt==e.rule.end()?0:number(countIt->second);if(countIt!=e.rule.end()&&(remaining<1||remaining>100000))throw std::runtime_error("Unsupported recurrence count");
            if(countIt!=e.rule.end()){if((expansionEnd-base).count()>36600)throw std::runtime_error("Recurrence exceeds expansion bound");first=base;}
            if(first<base)first=base;std::optional<sys_seconds> until;auto untilIt=e.rule.find("UNTIL");if(untilIt!=e.rule.end())until=absolute(stamp(untilIt->second,e.start.zone));
            for(auto candidate=first;candidate<=expansionEnd;candidate+=days{1}){if(++expansionSteps>250000)throw std::runtime_error("Calendar expansion budget exceeded");if(!matches(e,candidate))continue;Stamp occurrence=e.start;occurrence.local=candidate+offset;if(until&&absolute(occurrence)>*until)break;emit(occurrence);if(countIt!=e.rule.end()&&--remaining<=0)break;}
        }
        for(auto& extra:e.extra)emit(extra);
    }catch(...){++out.unsupported;}}
    std::stable_sort(out.entries.begin(),out.entries.end(),[](auto& a,auto& b){return a.allDay!=b.allDay?a.allDay:a.minute<b.minute;});return out;
}
}
