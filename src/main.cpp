#include "MainApplication.h"

#include "win32/Process.h"

auto WINAPI WinMain(_In_ HINSTANCE instanceHandle, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int /*showCommand*/) -> int {
    win32::Process::current_setDpiAwareness(win32::DpiAwareness::PerMonitorAwareV2);
    {
        const auto hr = CoInitialize(nullptr);
        if (!SUCCEEDED(hr)) return -1;
    }
    auto app = deskdup::MainApplication{instanceHandle};
    return app.run();
}
