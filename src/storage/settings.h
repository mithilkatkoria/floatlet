#pragma once
#include <string>
#include <vector>
#include <filesystem>
namespace delight {
struct Settings { int alignment=1; bool touch=false; bool followPointer=true; bool iconOnly=false; bool reduceMotion=false; bool callControls=true; std::vector<std::wstring> paths; };
Settings load(const std::filesystem::path& file);
void save(const std::filesystem::path& file,const Settings& settings);
bool addReference(Settings& s,const std::wstring& path);
}
