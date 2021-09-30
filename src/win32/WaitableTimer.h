#pragma once
#include "Handle.h"

#include <meta/member_method.h>

#include <Windows.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace win32 {

using Name = std::wstring;
using Milliseconds = std::chrono::milliseconds;
using HundredNanoSeconds =
    std::chrono::duration<int64_t, std::ratio_multiply<std::hecto, std::nano>>;
using TimePoint = std::chrono::system_clock::time_point;

/// Wrapper for a win32 WaitableTimer handle
struct WaitableTimer {
    struct Config {
        LPSECURITY_ATTRIBUTES securityAttributes = {};
        Name timerName = {};
        bool manualReset = {};
        DWORD desiredAccess = {TIMER_ALL_ACCESS};
    };
    WaitableTimer(Config);

    auto handle() -> HANDLE { return m_handle.get(); }

    struct SetArgs {
        std::variant<TimePoint, HundredNanoSeconds> time = {};
        std::optional<Milliseconds> period = {};
        // WakeContext??
        Milliseconds tolerableDelay = {};
    };
    template<auto M>
    requires(MemberMethod<decltype(M)>) bool set(
        SetArgs setArgs, MemberMethodClassArgsTuple<M> &&args) {
        using ArgsTuple = MemberMethodClassArgsTuple<M>;
        struct Helper {
            static void CALLBACK
            apc(LPVOID parameter, DWORD /*dwTimerLowValue*/, DWORD /*dwTimerHighValue*/) noexcept {
                auto tuplePtr =
                    std::unique_ptr<ArgsTuple>(reinterpret_cast<ArgsTuple *>(parameter));
                std::apply(M, *tuplePtr);
            }
        };
        auto parameter = std::make_unique<ArgsTuple>(std::move(args));
        return setImpl(setArgs, &Helper::apc, parameter.release());
    }

    void cancel();

private:
    bool setImpl(SetArgs, PTIMERAPCROUTINE, LPVOID);

private:
    Handle m_handle;
    Name m_timerName{};
};

} // namespace win32
