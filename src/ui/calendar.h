#pragma once
#include <chrono>
namespace delight {
struct Calendar {
    int year=2026; unsigned month=1,selected=1;
    unsigned count() const {using namespace std::chrono;return unsigned(year_month_day_last{std::chrono::year{year},month_day_last{std::chrono::month{month}}}.day());}
    unsigned offset() const {using namespace std::chrono;return weekday{sys_days{std::chrono::year{year}/std::chrono::month{month}/1}}.iso_encoding()-1;}
    void move(int months){using namespace std::chrono;auto next=std::chrono::year{year}/std::chrono::month{month}+std::chrono::months{months};if(int(next.year())<1601||int(next.year())>9998)return;year=int(next.year());month=unsigned(next.month());if(selected>count())selected=count();}
    unsigned dayAt(int cell) const {int d=cell-int(offset())+1;return d>0&&d<=int(count())?unsigned(d):0;}
};
}
