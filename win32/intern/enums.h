#pragma once
#include <inttypes.h>

namespace win32 {
namespace intern {

/*
 * ShowWindow() Commands
 */
enum class ShowWindowCmd : int32_t {
    Hide = 0,
    ShowNormal = 1,
    Normal = 1,
    ShowMinimized = 2,
    ShowMaximized = 3,
    Maximize = 3,
    ShowNoActivate = 4,
    Show = 5,
    Minimize = 6,
    ShowMinNoActive = 7,
    ShowNA = 8,
    Restore = 9,
    ShowDefault = 10,
    ForceMinimize = 11,
    Max = 11,
};

} // namespace intern
} // namespace win32
