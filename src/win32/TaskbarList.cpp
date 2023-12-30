#include "TaskbarList.h"

#if __has_include(<gsl.h>)
#    include <gsl.h>
#endif

namespace win32 {

MenuEntry::MenuEntry(const MenuEntry &o)
    : icon(DuplicateIcon(nullptr, o.icon))
    , bitmap(o.bitmap ? static_cast<HBITMAP>(CopyImage(o.bitmap, IMAGE_BITMAP, 0, 0, 0)) : HBITMAP{})
    , tooltip(o.tooltip)
    , flags(o.flags) {}

auto MenuEntry::operator=(const MenuEntry &o) -> MenuEntry & {
    if (icon) DestroyIcon(icon);
    if (bitmap) DeleteObject(bitmap);
    icon = o.icon ? DuplicateIcon(nullptr, o.icon) : HICON{};
    bitmap = o.bitmap ? static_cast<HBITMAP>(CopyImage(o.bitmap, IMAGE_BITMAP, 0, 0, 0)) : HBITMAP{};
    tooltip = o.tooltip;
    flags = o.flags;
    return *this;
}

TaskbarList::TaskbarList(HWND window, Config config)
    : m_window(window)
    , m_idBase(config.idBase)
    , m_iconSize(config.iconSize) {
    auto hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(p.GetAddressOf()));
    if (SUCCEEDED(hr)) {
        p->HrInit();
    }
}

auto TaskbarList::forWindow(HWND window) const noexcept -> TaskbarList {
    auto r = TaskbarList{};
    r.m_window = window;
    r.p = p;
    return r;
}

void TaskbarList::setProgressFlags(ProgressFlags flags) {
    auto f = TBPFLAG{};
    if (flags.has_any(ProgressFlag::Normal)) f |= TBPF_NORMAL;
    if (flags.has_any(ProgressFlag::Paused)) f |= TBPF_PAUSED;
    if (flags.has_any(ProgressFlag::Error)) f |= TBPF_ERROR;
    if (flags.has_any(ProgressFlag::Indeterminate)) f |= TBPF_INDETERMINATE;
    if (p) p->SetProgressState(m_window, f);
}

void TaskbarList::setProgressValue(uint64_t value, uint64_t total) {
    if (p) p->SetProgressValue(m_window, value, total);
}

void TaskbarList::setButtonLetterIcon(size_t idx, wchar_t chr, COLORREF Color) noexcept {
    const auto size = m_iconSize;
    constexpr const char fontName[] = "Segoe MDL2 Assets";
    auto dc = GetDC(m_window);
    auto dcMem = CreateCompatibleDC(dc);

    auto bitmap = CreateCompatibleBitmap(dc, size, size);

    ReleaseDC(m_window, dc);
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
    const auto oldTextColor = SetTextColor(dcMem, Color);

    const UINT format = DT_NOCLIP | DT_CENTER | DT_SINGLELINE | DT_VCENTER;
    DrawTextW(dcMem, &chr, 1, &rect, format);

    SetBkMode(dcMem, oldBkMode);
    // SetBkColor(dcMem, oldBkColor);
    SetTextColor(dcMem, oldTextColor);
    SelectObject(dcMem, oldFont);
    SelectObject(dcMem, oldBitmap);
    DeleteObject(font);

    if (m_menu[idx].bitmap) DeleteObject(m_menu[idx].bitmap);
    m_menu[idx].bitmap = bitmap;
    m_imageUpdated = true;

    DeleteDC(dcMem);
}

void TaskbarList::setButtonFlags(size_t idx, ThumbButtonFlags flags) noexcept {
    if (idx < m_menu.size()) {
        m_menu[idx].flags = flags;
    }
}

void TaskbarList::setButtonTooltip(size_t idx, std::wstring str) {
    if (idx < m_menu.size()) {
        m_menu[idx].tooltip = std::move(str);
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

void TaskbarList::updateThumbButtons() {
    if (m_imageUpdated) {
        updateThumbImages();
        m_imageUpdated = false;
    }

    constexpr auto menuSize = 7u;
    auto menuData = std::array<THUMBBUTTON, menuSize>{};
    auto i = 0u;
    for (auto &entry : m_menu) {
        [[gsl::suppress("26482")]] // menu and menuData have the same size
        auto &data = menuData[i];
        data.iId = i + m_idBase;
        data.dwFlags = toWindowsFlags(entry.flags);
        data.dwMask = THB_FLAGS | THB_TOOLTIP;
        [[gsl::suppress("26485")]] // this is save
        wcscpy_s(data.szTip, ARRAYSIZE(data.szTip), entry.tooltip.c_str());
        data.hIcon = entry.icon;
        if (entry.icon) data.dwMask |= THB_ICON;
        if (entry.bitmap) {
            data.iBitmap = i;
            data.dwMask |= THB_BITMAP;
        }
        i++;
    }

    if (!m_menuInitialized) {
        const auto hr = p->ThumbBarAddButtons(m_window, menuSize, menuData.data());
        (void)hr;
        m_menuInitialized = true;
    }
    else {
        p->ThumbBarUpdateButtons(m_window, menuSize, menuData.data());
    }
}

void TaskbarList::updateThumbImages() {
    auto imageList = ImageList_Create(m_iconSize, m_iconSize, ILC_MASK | ILC_COLOR32, 0, 8);
    for (auto &entry : m_menu) {
        ImageList_AddMasked(imageList, entry.bitmap, RGB(0, 0, 0));
    }
    p->ThumbBarSetImageList(m_window, imageList);
}

} // namespace win32
