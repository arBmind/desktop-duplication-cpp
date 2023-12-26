#pragma once
#include "meta/member_method.h"

#include <Windows.h>

#include <chrono>
#include <vector>

namespace win32 {

using Milliseconds = std::chrono::milliseconds;

struct ThreadLoop {
    enum Keep { Yes, Remove };
    struct Callback {
        template<class T>
        constexpr static auto make(T *cb) -> Callback {
            return {
                [](void *ptr, HANDLE handle) -> Keep {
                    auto p = std::bit_cast<T *>(ptr);
                    return p(handle);
                },
                cb};
        }
        template<auto M>
            requires(MemberMedthodSig<Keep(HANDLE), decltype(M)>)
        constexpr static auto makeMember(MemberMethodClass<M> *obj) -> Callback {
            Func *func = [](void *ptr, HANDLE handle) -> Keep {
                auto p = std::bit_cast<MemberMethodClass<M> *>(ptr);
                return (p->*M)(handle);
            };
            return {func, obj};
        }

        auto operator()(HANDLE h) const -> Keep { return m_func(m_ptr, h); }

    private:
        using Func = auto(void *, HANDLE) -> Keep;
        static auto noopFunc(void *, HANDLE) -> Keep { return Keep::Remove; }

        constexpr Callback(Func *func, void *ptr)
            : m_func(func)
            , m_ptr(ptr) {}

        Func *m_func = &Callback::noopFunc;
        void *m_ptr{};
    };

    bool isQuit() const { return m_quit; }
    bool keepRunning() const { return !m_quit; }
    int returnValue() const { return m_returnValue; }

    template<class T>
    void addAwaitable(HANDLE h, T *cb) {
        m_waitHandles.push_back(h);
        m_callbacks.push_back(Callback::make(cb));
    }
    template<auto M, class T>
    void addAwaitableMember(HANDLE h, T *cb) {
        m_waitHandles.push_back(h);
        m_callbacks.push_back(Callback::makeMember<M>(cb));
    }
    void removeAwaitable(HANDLE h) {
        auto it = std::find(m_waitHandles.begin(), m_waitHandles.end(), h);
        if (it == m_waitHandles.end()) return;
        m_callbacks.erase(m_callbacks.begin() + (it - m_waitHandles.begin()));
        m_waitHandles.erase(it);
    }

    void enableAllInputs() { m_queueStatus |= QS_ALLINPUT; }
    void enableAwaitAlerts() { m_awaitAlerts = true; }
    void quit() { m_quit = true; }

    int run() {
        while (keepRunning()) {
            sleep();
        }
        return returnValue();
    }

    void sleep();
    void processCurrentMessages();

private:
    std::vector<HANDLE> m_waitHandles;
    std::vector<Callback> m_callbacks;
    Milliseconds m_timeout{INFINITE};
    DWORD m_queueStatus{QS_ALLINPUT};
    bool m_awaitAlerts{};
    bool m_quit{};
    int m_returnValue{};
};

} // namespace win32
