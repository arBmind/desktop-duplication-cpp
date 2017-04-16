#pragma once
#include <inttypes.h>

#include "meta/flags.h"

namespace win32 {
namespace intern {

enum class WindowStyle : uint32_t {
    None              = 0,
    Overlapped        = 0x00000000,
    Popup             = 0x80000000,
    Child             = 0x40000000,
    Minimize          = 0x20000000,
    Visible           = 0x10000000,
    Disabled          = 0x08000000,
    Clipsiblings      = 0x04000000,
    ClipChildren      = 0x02000000,
    Maximize          = 0x01000000,
    Caption           = 0x00C00000,
    Border            = 0x00800000,
    DialogFrame       = 0x00400000,
    VerticalScroll    = 0x00200000,
    HorizontalScroll  = 0x00100000,
    SystemMenu        = 0x00080000,
    ThickFrame        = 0x00040000,
    Group             = 0x00020000,
    TabStop           = 0x00010000,
    MinimizeBox       = 0x00020000,
    MaximizeBox       = 0x00010000,
    Tiled = Overlapped,
    Iconic = Minimize,
    SizeBox = ThickFrame,
    OverlappedWindow = (Overlapped | Caption | SystemMenu | ThickFrame | MinimizeBox | MaximizeBox),
    TiledWindow = OverlappedWindow,
    Popupwindow = (Popup | Border | SystemMenu),
    ChildWindow = (Child),
};
using WindowStyles = meta::flags<WindowStyle>;

enum class ExWindowStyle : uint32_t {
    None                = 0,
    DialogModalFrame    = 0x00000001,
    NoParentNotify      = 0x00000004,
    TopMost             = 0x00000008,
    AcceptFiles         = 0x00000010,
    Transparent         = 0x00000020,
    MdiChild            = 0x00000040,
    ToolWindow          = 0x00000080,
    WindowEdge          = 0x00000100,
    ClientEdge          = 0x00000200,
    ContextHelp         = 0x00000400,
    Right               = 0x00001000,
    Left                = 0x00000000,
    RTLReading          = 0x00002000,
    LTRReading          = 0x00000000,
    LeftScrollbar       = 0x00004000,
    RightScrollbar      = 0x00000000,
    ControlParent       = 0x00010000,
    StaticEdge          = 0x00020000,
    ApplicationWindow   = 0x00040000,
    OverlappedWindow    = (WindowEdge | ClientEdge),
    PaletteWindow       = (WindowEdge | ToolWindow | TopMost),
    Layered             = 0x00080000,
    NoInheritLayout     = 0x00100000,
    NoRedirectionBitmap = 0x00200000,
    LayoutRTL           = 0x00400000,
    Composited          = 0x02000000,
    NoActivate          = 0x08000000,
};
using ExWindowStyles = meta::flags<ExWindowStyle>;

enum class WindowClassStyle : uint32_t {
    VerticalRedraw = 0x0001,
    HorizontalRedraw = 0x0002,
    DoubleClicks = 0x0008,
    OwnDC = 0x0020,
    ClassDC = 0x0040,
    ParentDC = 0x0080,
    NoClose = 0x0200,
    SaveBits = 0x0800,
    ByteAlignClient = 0x1000,
    ByteAlignWindow = 0x2000,
    GlobalClass = 0x4000,
    IME = 0x00010000,
    DropShadow = 0x00020000,
};
using WindowClassStyles = meta::flags<WindowClassStyle>;

}
}
