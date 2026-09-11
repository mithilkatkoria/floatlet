#include "services/ical.h"
#include "services/calendar_feed.h"
#include "ui/countdown.h"
#include <iostream>
using namespace delight;
int checks=0;void require(bool good){++checks;if(!good)throw std::runtime_error("Calendar/timer assertion failed");}
auto parse(std::string text){return ical::parse("BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"+text+"END:VCALENDAR\r\n");}
size_t count(const ical::Calendar& c,unsigned d,unsigned m=9,int y=2026){return ical::day(c,std::chrono::year{y}/m/d).entries.size();}
int main(){try{
    Countdown t;t.start(200,1000);require(t.text(1000)==L"3:20");require(t.text(1999)==L"3:20");require(t.text(2000)==L"3:19");t.pause(21000);require(t.left(999999)==180000);t.resume(50000);require(t.left(50000)==180000);require(!t.tick(229999));require(t.tick(230000));require(!t.tick(240000));require(t.finished&&!t.running);t.cancel();require(!t.active()&&!t.finished);t.start(999999,0);require(t.total==86400000);t.start(0,0);require(t.total==1000);
    auto c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260901T090000\r\nDTEND:20260901T100000\r\nSUMMARY:Design\\, review\r\n with team\r\nRRULE:FREQ=DAILY;COUNT=3\r\nEND:VEVENT\r\n");require(count(c,1)==1&&count(c,3)==1&&count(c,4)==0);require(c.events[0].title=="Design, reviewwith team");
    c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260907T090000\r\nRRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE\r\nEXDATE:20260921T090000\r\nEND:VEVENT\r\n");require(count(c,7)==1);require(count(c,9)==1);require(count(c,14)==0);require(count(c,21)==0);require(count(c,23)==1);
    c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART;VALUE=DATE:20260901\r\nDTEND;VALUE=DATE:20260903\r\nSUMMARY:Off site\r\nEND:VEVENT\r\n");require(count(c,1)==1&&count(c,2)==1&&count(c,3)==0);
    c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260914T120000\r\nRRULE:FREQ=MONTHLY;BYDAY=2MO\r\nEND:VEVENT\r\n");require(count(c,14)==1&&count(c,7)==0&&count(c,12,10)==1);
    c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260901T090000\r\nRRULE:FREQ=DAILY;COUNT=3\r\nEND:VEVENT\r\nBEGIN:VEVENT\r\nUID:a\r\nRECURRENCE-ID:20260903T090000\r\nDTSTART:20260905T110000\r\nEND:VEVENT\r\n");require(count(c,3)==0&&count(c,5)==1);
    c=parse("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260901T090000\r\nRRULE:FREQ=MONTHLY;BYDAY=MO;BYSETPOS=1\r\nEND:VEVENT\r\n");auto unsupported=ical::day(c,std::chrono::year{2026}/9/7);require(unsupported.entries.empty()&&unsupported.unsupported==1);
    using namespace std::chrono;auto london=ical::stamp("20260925T120000","Europe/London");require(ical::absolute(london)==sys_days{year{2026}/9/25}+hours{11});auto winter=ical::stamp("20261225T120000","Europe/London");require(ical::absolute(winter)==sys_days{year{2026}/12/25}+hours{12});
    bool rejected=false;try{ical::stamp("20260230T090000");}catch(...){rejected=true;}require(rejected);
    require(CalendarFeed::validUrl(L"https://calendar.google.com/calendar/ical/example/basic.ics"));require(!CalendarFeed::validUrl(L"https://calendar.google.com.evil.test/calendar/ical/basic.ics"));require(!CalendarFeed::validUrl(L"http://calendar.google.com/calendar/ical/basic.ics"));require(!CalendarFeed::validUrl(L"https://calendar.google.com/calendar/ical/basic.ics\r\nInjected"));
    std::cout<<checks<<" timer, calendar recurrence, timezone and subscription-boundary checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<" at "<<checks<<'\n';return 1;}}
