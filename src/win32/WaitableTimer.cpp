#include "WaitableTimer.h"

namespace win32 {

WaitableTimer::WaitableTimer(Config config)
    : m_timerName(std::move(config.timerName)) {
    auto securityAttributes = nullptr;
    auto flags = config.manualReset ? CREATE_WAITABLE_TIMER_MANUAL_RESET : 0u;
    m_handle.reset(::CreateWaitableTimerExW(securityAttributes, m_timerName.data(), flags, config.desiredAccess));
}

void WaitableTimer::cancel() { ::CancelWaitableTimer(m_handle.get()); }

bool WaitableTimer::setImpl(SetArgs const &args, PTIMERAPCROUTINE callback, LPVOID callbackPtr) {
    auto queueTime = LARGE_INTEGER{};
    std::visit(
        [&]<class T>(const T &v) {
            if constexpr (std::is_same_v<T, TimePoint>) {
                queueTime.QuadPart =
                    std::chrono::duration_cast<HundredNanoSeconds>(v.time_since_epoch()).count() + 116444736000000000;
            }
            else if constexpr (std::is_same_v<T, HundredNanoSeconds>) {
                queueTime.QuadPart = -v.count();
            }
            else {
                static_assert(sizeof(T) != sizeof(T), "Missing!");
            }
        },
        args.time);
    auto period = ULONG(args.period ? args.period->count() : 0u);
    auto tolerableDelay = static_cast<ULONG>(args.tolerableDelay.count());
    auto success = ::SetWaitableTimerEx(handle(), &queueTime, period, callback, callbackPtr, nullptr, tolerableDelay);
    return success != 0;
}

} // namespace win32
