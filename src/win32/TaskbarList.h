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

struct MenuEntry {
    HICON icon{};
    HBITMAP bitmap{};
    std::wstring tooltip;
    ThumbButtonFlags flags{ThumbButtonFlag::Hidden};

    MenuEntry() = default;
    MenuEntry(const MenuEntry &o);
    auto operator=(const MenuEntry &) -> MenuEntry &;
    MenuEntry(MenuEntry &&o) = default;
    auto operator=(MenuEntry &&o) -> MenuEntry & = default;
    ~MenuEntry() = default;
};
using menu_entries = std::array<MenuEntry, 7>;

struct TaskbarList {
    struct Config {
        UINT idBase = 0u;
        int iconSize = 24;
    };

    TaskbarList() = default;
    TaskbarList(HWND window)
        : TaskbarList(window, Config{}) {}
    TaskbarList(HWND window, Config);

    auto forWindow(HWND window) const noexcept -> TaskbarList;

    void setProgressFlags(ProgressFlags = ProgressFlag::Disabled);
    void setProgressValue(uint64_t value, uint64_t total = 100);

    void setButtonLetterIcon(size_t idx, wchar_t chr, COLORREF Color = RGB(255, 255, 255)) noexcept;
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
