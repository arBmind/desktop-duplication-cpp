#include "taskbar_list.h"

#include <CppCoreCheck/Warnings.h>
#include <gsl.h>

namespace win32 {

menu_entry::menu_entry(const menu_entry::this_t &o)
    : icon(DuplicateIcon(nullptr, o.icon))
    , bitmap(
          o.bitmap ? static_cast<HBITMAP>(CopyImage(o.bitmap, IMAGE_BITMAP, 0, 0, 0)) : HBITMAP{})
    , tooltip(o.tooltip)
    , flags(o.flags) {}

menu_entry::this_t &menu_entry::operator=(const menu_entry::this_t &o) {
    if (icon) DestroyIcon(icon);
    if (bitmap) DeleteObject(bitmap);
    icon = o.icon ? DuplicateIcon(nullptr, o.icon) : HICON{};
    bitmap =
        o.bitmap ? static_cast<HBITMAP>(CopyImage(o.bitmap, IMAGE_BITMAP, 0, 0, 0)) : HBITMAP{};
    tooltip = o.tooltip;
    flags = o.flags;
    return *this;
}

taskbar_list::taskbar_list(HWND window, config config)
    : window(window)
    , idBase(config.idBase)
    , iconSize(config.iconSize) {
    auto hr = CoCreateInstance(
        CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(p.GetAddressOf()));
    if (SUCCEEDED(hr)) {
        hr = p->HrInit();
    }
}

taskbar_list taskbar_list::forWindow(HWND window) const noexcept {
    auto r = taskbar_list{};
    r.window = window;
    r.p = p;
    return r;
}

void taskbar_list::setProgressFlags(ProgressFlags flags) {
    auto f = TBPFLAG{};
    if (flags.has_any(ProgressFlag::Normal)) f |= TBPF_NORMAL;
    if (flags.has_any(ProgressFlag::Paused)) f |= TBPF_PAUSED;
    if (flags.has_any(ProgressFlag::Error)) f |= TBPF_ERROR;
    if (flags.has_any(ProgressFlag::Indeterminate)) f |= TBPF_INDETERMINATE;
    if (p) p->SetProgressState(window, f);
}

void taskbar_list::setProgressValue(uint64_t value, uint64_t total) {
    if (p) p->SetProgressValue(window, value, total);
}

void taskbar_list::setButtonLetterIcon(size_t idx, wchar_t chr, COLORREF color) noexcept {
    const auto size = iconSize;
    constexpr const char fontName[] = "Segoe MDL2 Assets";
    auto dc = GetDC(window);
    auto dcMem = CreateCompatibleDC(dc);

    auto bitmap = CreateCompatibleBitmap(dc, size, size);

    ReleaseDC(window, dc);
    DeleteDC(dc);

    const auto fontHeight = size;
    const auto fontWidth = 0;
    const auto escapement = 0;
    const auto orientation = 0;
    const auto weight = 900;
    const auto italic = false;
    const auto underline = false;
    const auto strikeOut = false;
    const auto charSet = 0u;
    const DWORD outputPrecision = OUT_TT_PRECIS;
    const DWORD clipPrecision = CLIP_DEFAULT_PRECIS;
    const DWORD quality = NONANTIALIASED_QUALITY;
    const DWORD pitchAndFamily = FF_DONTCARE;
    const auto font = CreateFontA(
        fontHeight,
        fontWidth,
        escapement,
        orientation,
        weight,
        italic,
        underline,
        strikeOut,
        charSet,
        outputPrecision,
        clipPrecision,
        quality,
        pitchAndFamily,
        fontName);
    const auto oldFont = SelectObject(dcMem, font);

    const auto oldBitmap = SelectObject(dcMem, bitmap);
    // SetDCBrushColor(dcMem, RGB(0, 0, 0));
    // PatBlt(dcMem, 0, 0, size, size, PATCOPY);

    auto rect = RECT{0, 0, size, size};
    const auto brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dcMem, &rect, brush);

    const auto oldBkMode = SetBkMode(dcMem, TRANSPARENT);
    // auto oldBkColor = SetBkColor(dcMem, RGB(0, 0, 0));
    const auto oldTextColor = SetTextColor(dcMem, color);

    const UINT format = DT_NOCLIP | DT_CENTER | DT_SINGLELINE | DT_VCENTER;
    DrawTextW(dcMem, &chr, 1, &rect, format);

    SetBkMode(dcMem, oldBkMode);
    // SetBkColor(dcMem, oldBkColor);
    SetTextColor(dcMem, oldTextColor);
    SelectObject(dcMem, oldFont);
    SelectObject(dcMem, oldBitmap);
    DeleteObject(font);

    if (menu[idx].bitmap) DeleteObject(menu[idx].bitmap);
    menu[idx].bitmap = bitmap;
    image_updated = true;

    DeleteDC(dcMem);
}

void taskbar_list::setButtonFlags(size_t idx, ThumbButtonFlags flags) noexcept {
    if (idx < menu.size()) {
        menu[idx].flags = flags;
    }
}

void taskbar_list::setButtonTooltip(size_t idx, std::wstring str) {
    if (idx < menu.size()) {
        menu[idx].tooltip = std::move(str);
    }
}

static auto toWindowsFlags(ThumbButtonFlags flags) -> THUMBBUTTONFLAGS {
    THUMBBUTTONFLAGS f = THBF_ENABLED;
    if (flags.has_any(ThumbButtonFlag::Disabled)) f |= THBF_DISABLED;
    if (flags.has_any(ThumbButtonFlag::DismissOnClick)) f |= THBF_DISMISSONCLICK;
    if (flags.has_any(ThumbButtonFlag::NoBackground)) f |= THBF_NOBACKGROUND;
    if (flags.has_any(ThumbButtonFlag::Hidden)) f |= THBF_HIDDEN;
    if (flags.has_any(ThumbButtonFlag::NonInteractive)) f |= THBF_NONINTERACTIVE;
    return f;
}

void taskbar_list::updateThumbButtons() {
    if (image_updated) {
        updateThumbImages();
        image_updated = false;
    }

    constexpr auto menuSize = 7u;
    auto menuData = std::array<THUMBBUTTON, menuSize>{};
    auto i = 0u;
    for (auto &entry : menu) {
        [[gsl::suppress(26482)]] // menu and menuData have the same size
            auto &data = menuData[i];
        data.iId = i + idBase;
        data.dwFlags = toWindowsFlags(entry.flags);
        data.dwMask = THB_FLAGS | THB_TOOLTIP;
        [[gsl::suppress(26485)]] // this is save
            wcscpy_s(data.szTip, ARRAYSIZE(data.szTip), entry.tooltip.c_str());
        data.hIcon = entry.icon;
        if (entry.icon) data.dwMask |= THB_ICON;
        if (entry.bitmap) {
            data.iBitmap = i;
            data.dwMask |= THB_BITMAP;
        }
        i++;
    }

    if (!menu_initialized) {
        const auto hr = p->ThumbBarAddButtons(window, menuSize, menuData.data());
        menu_initialized = true;
    }
    else {
        p->ThumbBarUpdateButtons(window, menuSize, menuData.data());
    }
}

void taskbar_list::updateThumbImages() {
    auto imageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLOR32, 0, 8);
    for (auto &entry : menu) {
        ImageList_AddMasked(imageList, entry.bitmap, RGB(0, 0, 0));
    }
    p->ThumbBarSetImageList(window, imageList);
}

} // namespace win32
