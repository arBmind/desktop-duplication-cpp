#pragma once
#include "stable.h"

#include "meta/flags.h"
#include <meta/comptr.h>

#include <ShObjIdl.h>

#include <inttypes.h>

namespace win32 {

enum class progress {
    Disabled = 0,
    Indeterminate = 1,
    Normal = 2,
    Error = 4,
    Paused = 8,
};
using ProgressFlags = meta::flags<progress>;
META_FLAGS_OP(progress)

struct taskbar_list {
    using this_t = taskbar_list;
    taskbar_list() = default;
    taskbar_list(HWND window);

    taskbar_list(const this_t &o) = default;
    this_t &operator=(const this_t &) = default;
    taskbar_list(this_t &&o) = default;
    this_t &operator=(this_t &&o) = default;

    taskbar_list forWindow(HWND window) const;

    void setProgressFlags(ProgressFlags = progress::Disabled);
    void setProgressValue(uint64_t value, uint64_t total = 100);

private:
    HWND window{};
    ComPtr<ITaskbarList3> p{};
};

} // namespace win32
