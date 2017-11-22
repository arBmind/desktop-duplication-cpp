#include "taskbar_list.h"

namespace win32 {

taskbar_list::taskbar_list(HWND window)
    : window(window) {
    auto hr = CoCreateInstance(
        CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(p.GetAddressOf()));
    if (SUCCEEDED(hr)) {
        hr = p->HrInit();
    }
}

taskbar_list taskbar_list::forWindow(HWND window) const {
    auto r = taskbar_list{};
    r.window = window;
    r.p = p;
    return r;
}

void taskbar_list::setProgressFlags(ProgressFlags flags) {
    auto f = TBPFLAG{};
    if (flags.has_all(progress::Normal)) f |= TBPF_NORMAL;
    if (flags.has_all(progress::Paused)) f |= TBPF_PAUSED;
    if (flags.has_all(progress::Error)) f |= TBPF_ERROR;
    if (flags.has_all(progress::Indeterminate)) f |= TBPF_INDETERMINATE;
    if (p) p->SetProgressState(window, f);
}

void taskbar_list::setProgressValue(uint64_t value, uint64_t total) {
    if (p) p->SetProgressValue(window, value, total);
}

} // namespace win32
