#pragma once

#include "stable.h"

#include <windowsx.h>

#include <utility>

namespace win32 {

namespace {

auto createPowerRequestHandle(const wchar_t *reasonString) noexcept -> HANDLE {
    REASON_CONTEXT reason;
    reason.Version = POWER_REQUEST_CONTEXT_VERSION;
    reason.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    [[gsl::suppress(26492)]] // the C API is not const correct!
        reason.Reason.SimpleReasonString = const_cast<wchar_t *>(reasonString);
    return PowerCreateRequest(&reason);
}

} // namespace

template<POWER_REQUEST_TYPE... required>
struct power_request {
    using this_t = power_request;

    power_request() = default;
    power_request(const wchar_t *reason) noexcept
        : handle(createPowerRequestHandle(reason)) {}
    ~power_request() {
        if (was_set) clear();
        CloseHandle(handle);
    }

    power_request(const this_t &) = delete;
    auto operator=(const this_t &) -> this_t & = delete;
    power_request(this_t &&o) noexcept
        : was_set(o.was_set)
        , handle(o.handle) {
        o.reset();
    }
    auto operator=(this_t &&o) noexcept -> this_t & {
        if (was_set) clear();
        CloseHandle(handle);
        handle = o.handle;
        was_set = o.was_set;
        o.reset();
        return *this;
    }

    bool set() noexcept {
        if (was_set) return true;
        was_set = true;
        bool r = true;
        const auto x = {((r = r && PowerSetRequest(handle, required)), 0)...};
        return r;
    }

    bool clear() noexcept {
        if (!was_set) return true;
        was_set = false;
        bool r = true;
        const auto x = {((r = r && PowerClearRequest(handle, required)), 0)...};
        return r;
    }

private:
    void reset() noexcept {
        was_set = false;
        handle = INVALID_HANDLE_VALUE;
    }

private:
    bool was_set = false;
    HANDLE handle = INVALID_HANDLE_VALUE;
};

} // namespace win32
