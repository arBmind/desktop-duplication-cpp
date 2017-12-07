#pragma once
#include "stable.h"

#include "meta/flags.h"
#include <meta/comptr.h>

#include <ShObjIdl.h>

#include <array>
#include <inttypes.h>
#include <string>

namespace win32 {

enum class ProgressFlag {
    Disabled = 0,
    Indeterminate = 1,
    Normal = 2,
    Error = 4,
    Paused = 8,
};
using ProgressFlags = meta::flags<ProgressFlag>;
META_FLAGS_OP(ProgressFlag)

enum class ThumbButtonFlag {
    Enabled = 0,
    Disabled = 1 << 0,
    DismissOnClick = 1 << 1,
    NoBackground = 1 << 2,
    Hidden = 1 << 3,
    NonInteractive = 1 << 4,
};
using ThumbButtonFlags = meta::flags<ThumbButtonFlag>;
META_FLAGS_OP(ThumbButtonFlag)

struct menu_entry {
    using this_t = menu_entry;

    HICON icon{};
    HBITMAP bitmap{};
    std::wstring tooltip;
    ThumbButtonFlags flags{ThumbButtonFlag::Hidden};

    menu_entry() = default;
    menu_entry(const this_t &o);
    this_t &operator=(const this_t &);
    menu_entry(this_t &&o) = default;
    this_t &operator=(this_t &&o) = default;
    ~menu_entry() = default;
};
using menu_entries = std::array<menu_entry, 7>;

struct taskbar_list {
    struct config {
        UINT idBase = 0;
        int iconSize = 24;
    };

    using this_t = taskbar_list;
    taskbar_list() = default;
    taskbar_list(HWND window, config = {});

    taskbar_list(const this_t &o) = default;
    this_t &operator=(const this_t &) = default;
    taskbar_list(this_t &&o) = default;
    this_t &operator=(this_t &&o) = default;
    ~taskbar_list() = default;

    taskbar_list forWindow(HWND window) const noexcept;

    void setProgressFlags(ProgressFlags = ProgressFlag::Disabled);
    void setProgressValue(uint64_t value, uint64_t total = 100);

    void setButtonLetterIcon(size_t idx, wchar_t chr, COLORREF color = RGB(255, 255, 255)) noexcept;
    void setButtonTooltip(size_t idx, std::wstring = {});
    void setButtonFlags(size_t idx, ThumbButtonFlags = ThumbButtonFlag::Hidden) noexcept;

    void updateThumbButtons();

private:
    void updateThumbImages();

private:
    HWND window{};
    UINT idBase = 1;
    int iconSize = 24;
    ComPtr<ITaskbarList3> p{};
    menu_entries menu{};
    bool menu_initialized = false;
    bool image_updated = false;
};

} // namespace win32
