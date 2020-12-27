#pragma once
#include "Handle.h"

#include "meta/member_method.h"
#include "meta/tuple.h"

namespace win32 {

/// Wrapper for win32 Threads
/// note: handle is owned by this!
struct Thread {
    explicit Thread() = default;
    explicit Thread(HANDLE handle)
        : m_threadHandle{handle} {}

    static auto fromCurrent() -> Thread;

    auto handle() const -> HANDLE { return m_threadHandle.get(); }

    template<auto M>
    requires(MemberMedthod<decltype(M)>) void queueUserApc(MemberMethodClassArgsTuple<M> &&args) {
        using ArgsTuple = MemberMethodClassArgsTuple<M>;
        struct Helper {
            static void CALLBACK apc(ULONG_PTR parameter) noexcept {
                auto tuplePtr =
                    std::unique_ptr<ArgsTuple>(reinterpret_cast<ArgsTuple *>(parameter));
                std::apply(M, *tuplePtr);
            }
        };

        auto parameter = std::make_unique<ArgsTuple>(std::move(args));
        const auto success = ::QueueUserAPC(
            &Helper::apc, handle(), reinterpret_cast<ULONG_PTR>(parameter.release()));
        // if (!success) throw Unexpected{"api::setError failed to queue APC"};
    }

private:
    Handle m_threadHandle{};
};

} // namespace win32
