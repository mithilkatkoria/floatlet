#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include "model.h"
#include "options.h"
namespace delight::search {
class Launcher {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    Launcher(HWND owner,std::vector<Result> commands,std::function<void(int)> command,std::function<bool(const std::wstring&)> add);
    ~Launcher();
    void show();
    void dismiss();
    void configure(Options);
    std::string diagnostics() const;
    bool translate(MSG& message);
};
}
