#pragma once

namespace win32 {

enum class DpiAwareness {
    Unknown,
    Unaware,
    UnawareGdiScaled,
    SystemAware,
    PerMonitorAware,
    PerMonitorAwareV2,
};
struct Dpi {
    unsigned value;
};

} // namespace win32
