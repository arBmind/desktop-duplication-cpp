#include "Process.h"

namespace win32 {

void Process::current_setDpiAwareness(DpiAwareness dpiAwareness) {
    auto dpiAwarenessContext = [&]() -> DPI_AWARENESS_CONTEXT {
        switch (dpiAwareness) {
        case DpiAwareness::Unaware: return DPI_AWARENESS_CONTEXT_UNAWARE;
        case DpiAwareness::UnawareGdiScaled: return DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED;
        case DpiAwareness::SystemAware: return DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
        case DpiAwareness::PerMonitorAware: return DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE;
        case DpiAwareness::PerMonitorAwareV2: return DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
        case DpiAwareness::Unknown: return nullptr;
        }
        return nullptr;
    }();
    ::SetProcessDpiAwarenessContext(dpiAwarenessContext);
}

} // namespace win32
