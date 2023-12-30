#pragma once
#include <Windows.h>
#include <windowsx.h>

namespace win32 {

namespace {

auto createPowerRequestHandle(const wchar_t *reasonString) noexcept -> HANDLE {
    REASON_CONTEXT reason;
    reason.Version = POWER_REQUEST_CONTEXT_VERSION;
    reason.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    [[gsl::suppress("26492")]] // the C API is not const correct!
    reason.Reason.SimpleReasonString = const_cast<wchar_t *>(reasonString);
    return PowerCreateRequest(&reason);
}

} // namespace

template<POWER_REQUEST_TYPE... required>
struct PowerRequest {
    PowerRequest() = default;
    PowerRequest(const wchar_t *reason) noexcept
        : m_handle(createPowerRequestHandle(reason)) {}
    ~PowerRequest() {
        if (m_was_set) clear();
        CloseHandle(m_handle);
    }

    PowerRequest(const PowerRequest &) = delete;
    auto operator=(const PowerRequest &) -> PowerRequest & = delete;
    PowerRequest(PowerRequest &&o) noexcept
        : m_was_set(o.m_was_set)
        , m_handle(o.m_handle) {
        o.reset();
    }
    auto operator=(PowerRequest &&o) noexcept -> PowerRequest & {
        if (m_was_set) clear();
        CloseHandle(m_handle);
        m_handle = o.m_handle;
        m_was_set = o.m_was_set;
        o.reset();
        return *this;
    }

    bool set() noexcept {
        if (m_was_set) return true;
        m_was_set = true;
        return (PowerSetRequest(m_handle, required) && ...);
    }

    bool clear() noexcept {
        if (!m_was_set) return true;
        m_was_set = false;
        return (PowerClearRequest(m_handle, required) && ...);
    }

private:
    void reset() noexcept {
        m_was_set = false;
        m_handle = INVALID_HANDLE_VALUE;
    }

private:
    bool m_was_set = false;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

} // namespace win32
