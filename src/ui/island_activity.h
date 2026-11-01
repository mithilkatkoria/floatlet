#pragma once
namespace delight {
enum class IslandActivity { Automatic, Music, Timer, Stopwatch, Call, Alarm };
struct Activities { bool music=false, timer=false, stopwatch=false, call=false, timerAlert=false, alarmAlert=false; };
// One decision shared by drawing, hit testing, artwork and animation scheduling.
inline IslandActivity chooseActivity(IslandActivity preference, Activities a) {
    if(a.alarmAlert)return IslandActivity::Alarm;
    if(a.timerAlert)return IslandActivity::Timer;
    if((preference==IslandActivity::Music&&a.music)||(preference==IslandActivity::Timer&&a.timer)||
       (preference==IslandActivity::Stopwatch&&a.stopwatch)||(preference==IslandActivity::Call&&a.call))return preference;
    if(a.call)return IslandActivity::Call;
    if(a.timer)return IslandActivity::Timer;
    if(a.stopwatch)return IslandActivity::Stopwatch;
    if(a.music)return IslandActivity::Music;
    return IslandActivity::Automatic;
}
}
