#pragma once
#include "Handle.h"

#include "meta/callback_adapter.h"

namespace win32 {

/// Wrapper for win32 Threads
/// note: handle is owned by this!
struct Thread {
    explicit Thread() = default;
    explicit Thread(HANDLE handle)
        : m_threadHandle{handle} {}

    static auto fromCurrent() -> Thread;

    auto handle() const -> HANDLE { return m_threadHandle.get(); }

    template<class Functor>
    void queueUserApc(Functor&& functor) {
        auto callback = UniqueCallbackAdapter<ULONG_PTR>(std::forward<Functor>(functor));
        const auto success = ::QueueUserAPC(callback.c_callback(), handle(), callback.c_parameter());
        if (success) callback.release();
        // if (!success) throw Unexpected{"api::setError failed to queue APC"};
    }

private:
    Handle m_threadHandle{};
};

} // namespace win32
