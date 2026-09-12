#include "ui/time_tools.h"
#include "ui/countdown.h"
#include "services/chime.h"
#include "services/calendar_reminders.h"
#include <iostream>
#include <stdexcept>
using namespace delight;
int main(){int checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("time tools assertion failed");};try{
    Stopwatch sw;check(sw.text(100)==L"00:00");sw.toggle(1000);check(sw.elapsed(2500)==1500);check(sw.preciseText(2509)==L"00:01.50");sw.toggle(3500);check(sw.elapsed(100000)==2500);sw.toggle(200000);check(sw.elapsed(202500)==5000);sw.reset();check(!sw.running&&sw.elapsed(999999)==0);
    Countdown timer;timer.start(300,1000);check(timer.text(1000)==L"5:00");timer.pause(2000);check(timer.left(900000)==299000);timer.resume(1000000);check(!timer.tick(1298999));check(timer.tick(1299000));check(!timer.tick(1299001));check(timer.finished&&timer.fraction(1300000)==0);timer.cancel();check(!timer.finished&&!timer.active());
    using namespace std::chrono;auto zone=current_zone();auto now=system_clock::now();auto local=zone->to_local(now);auto day=floor<days>(local);auto noon=zone->to_sys(local_seconds{day}+hours{12},choose::latest);
    Alarm alarm;alarm.hour=13;alarm.minute=15;alarm.arm(noon);check(alarm.armed&&alarm.due>noon);check(!alarm.tick(alarm.due-seconds{1}));check(alarm.tick(alarm.due));check(!alarm.tick(alarm.due+seconds{1}));check(!alarm.armed&&alarm.ringing);alarm.cancel();check(!alarm.ringing);
    alarm.hour=11;alarm.minute=0;alarm.arm(noon);check(floor<days>(zone->to_local(alarm.due))==day+days{1});check(alarm.text()==L"11:00");alarm.hour=0;alarm.minute=5;check(alarm.text()==L"00:05");
    auto wave=makeChime();check(wave.size()==176444);check(std::memcmp(wave.data(),"RIFF",4)==0&&std::memcmp(wave.data()+8,"WAVE",4)==0);int peak=0;for(size_t i=44;i<wave.size();i+=2){std::int16_t value=std::int16_t(unsigned(wave[i])|(unsigned(wave[i+1])<<8));peak=std::max(peak,std::abs(int(value)));}check(peak>1000&&peak<32767);check(wave[44]==0&&wave[45]==0);
    Alarm persistent;persistent.armed=true;persistent.due=noon;check(persistent.tick(noon));check(!persistent.tick(noon+hours{12})&&persistent.ringing);
    Countdown persistentTimer;persistentTimer.start(1,0);check(persistentTimer.tick(1000));check(!persistentTimer.tick(86400000)&&persistentTimer.finished);
    CalendarReminders reminders;auto at=floor<seconds>(noon);ical::Entry entry{"Maths","",0,0,false,"maths",at+minutes{5}};
    check(!reminders.next({entry},at-seconds{1}));check(reminders.next({entry},at).has_value());check(!reminders.next({entry},at+minutes{1}));
    entry.start+=minutes{1};check(reminders.next({entry},at+minutes{1}).has_value());
    entry.uid="past";entry.start=at;check(!reminders.next({entry},at));entry.uid="all-day";entry.start=at+minutes{2};entry.allDay=true;check(!reminders.next({entry},at));
    check(!reminders.next({},at+hours{1}));check(reminders.shown.empty());
    std::cout<<checks<<" clock and reminder assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<" at "<<checks<<"\n";return 1;}}
