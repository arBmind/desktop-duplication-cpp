#pragma once
#include "Geometry.h"

#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM

#include <chrono>
#include <optional>
#include <stdint.h> // uint32_t

namespace win32 {

template<class T>
using Optional = std::optional<T>;

using OptLRESULT = Optional<LRESULT>;

using Milliseconds = std::chrono::milliseconds;

struct WindowMessage {
    HWND window = {};
    uint32_t type = {};
    WPARAM wParam = {};
    LPARAM lParam = {};
};
struct ControlCommand {
    WORD notificationCode{};
    WORD identifier{};
    LPARAM handle{};
};
struct AppCommand {
    WORD device; // FAPPCOMMAND_KEY | FAPPCOMMAND_MOUSE | FAPPCOMMAND_OEM
    WORD keys; // MK_CONTROL | MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_SHIFT | MK_XBUTTON1 |
               // MK_XBUTTON2
};

/// CRTP Wrapper to easily handle messages
/// usage:
///     struct MyHandler final : private MessageHandler<MyHandler> {
///     private:
///         friend struct MessageHandler<MyHandler>;
///         // override only the handlers you need!
///         void close() override;
///     };
/// note:
/// * we use virtual to allow using the override keyword
/// * all message methods are called directly without virtual dispatch!
template<class Actual>
struct MessageHandler {
#pragma warning(push)
#pragma warning(disable : 4100) // unereference parameter - its here for documentation only!
    virtual void create(CREATESTRUCT *) {}
    virtual void destroy() {}
    virtual void size(const Dimension &, uint32_t) {}
    virtual void move(const Point &topLeft) {}
    // virtual void activate() {}
    virtual void setFocus() {}
    virtual void killFocus() {}
    // virtual void enable() {}
    // virtual void setRedraw() {}
    // virtual void setText() {}
    // virtual void getText() {}
    // virtual void getTextLength() {}
    virtual bool paint() { return false; } //< return true only if painted!
    virtual void close() {}
    // virtual void queryEndSession() {}
    // virtual void queryOpen() {}
    // virtual void endSession() {}
    // virtual void quit() {}
    // virtual void eraseBackground() {}
    // virtual void systemColorChange() {}
    // virtual void showWindow() {}
    // virtual void windowsIniChange() {}
    // …
    virtual void displayChange(uint32_t bitsPerPixel, const Dimension &) {}

    // Input handling
    virtual void inputKeyDown(uint32_t keyCode) {}
    virtual void inputKeyUp(uint32_t keyCode) {}
    virtual void inputChar() {}
    // virtual void inputDeadChar() {}
    virtual void inputUnicodeChar() {}
    // …

    // Mouse handling
    virtual void mouseMove(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseLeftButtonDown(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseLeftButtonUp(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseLeftButtonDoubleClick(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseRightButtonDown(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseRightButtonUp(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseRightDoubleClick(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonDown(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonUp(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonDoubleClick(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) {}
    virtual void mouseExtraButtonDown() {}
    virtual void mouseExtraButtonUp() {}
    virtual void mouseExtraButtonDoubleClick() {}
    // …

    virtual void dpiChanged(const Rect &) {}

    virtual auto deviceChange(WPARAM event, LPARAM pointer) -> LRESULT { return {}; }

    virtual auto sysCommand(uint16_t command, const Point &mousePosition) -> OptLRESULT {
        return {};
    }
    virtual auto appCommand(uint32_t command, const AppCommand &) -> OptLRESULT { return {}; }
    virtual auto menuCommand(uint16_t command) -> OptLRESULT { return {}; }
    virtual auto acceleratorCommand(uint16_t command) -> OptLRESULT { return {}; }
    virtual auto controlCommand(const ControlCommand &) -> OptLRESULT { return {}; }

    virtual void timer(HWND hwnd, WPARAM timer, TIMERPROC timerProc) {
#pragma warning(suppress : 28159) // API requires GetTickCount!
        if (timerProc) timerProc(hwnd, WM_TIMER, timer, GetTickCount());
    }

    // user defined messages
    virtual auto userMessage(const WindowMessage &) -> OptLRESULT { return {}; }

    virtual void powerStatusChange() {}
    virtual void powerResumeAutomatic() {}
    virtual void powerResumeSuspend() {}
    virtual void powerSuspend() {}
    virtual void powerSettingChange(POWERBROADCAST_SETTING *) {}
#pragma warning(pop)
protected:
    /// mouse position at the occurence of the processed message
    static auto messageMousePosition() -> Point {
        auto pos = ::GetMessagePos();
        return {GET_X_LPARAM(pos), GET_Y_LPARAM(pos)};
    }

    /// time the message was send
    static auto messageTime() -> Milliseconds { return Milliseconds{::GetMessageTime()}; }

    /// construct modifier keys for messages that do not get them
    static auto modifierKeys() noexcept -> uint32_t {
        auto output = 0u;
        if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) output |= MK_SHIFT;
        if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0) output |= MK_CONTROL;
        if ((::GetKeyState(VK_MENU) & 0x8000) != 0) output |= MK_ALT;
        return output;
    }

private:
    friend struct WindowWithMessages;

    static auto handleMessage(void *actual, const WindowMessage &msg) -> OptLRESULT {
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (message) {
        case WM_CREATE:
            return m->Actual::create(reinterpret_cast<CREATESTRUCT *>(lParam)), LRESULT{};
        case WM_DESTROY: return m->Actual::destroy(), LRESULT{};
        case WM_SIZE:
            return m->Actual::size(
                       Dimension{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       static_cast<uint32_t>(wParam)),
                   LRESULT{};
        case WM_MOVE:
            return m->Actual::move(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}), LRESULT{};
        case WM_SETFOCUS: return m->Actual::setFocus(), LRESULT{};
        case WM_KILLFOCUS: return m->Actual::killFocus(), LRESULT{};

        case WM_PAINT: return m->Actual::paint() ? OptLRESULT{LRESULT{}} : OptLRESULT{};
        case WM_CLOSE: return m->Actual::close(), LRESULT{};

        case WM_KEYDOWN: return m->Actual::inputKeyDown(static_cast<uint32_t>(wParam)), LRESULT{};
        case WM_KEYUP: return m->Actual::inputKeyUp(static_cast<uint32_t>(wParam)), LRESULT{};

        case WM_MOUSEMOVE:
            return m->Actual::mouseMove(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONDOWN:
            return m->Actual::mouseLeftButtonDown(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONUP:
            return m->Actual::mouseLeftButtonUp(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONDBLCLK:
            return m->Actual::mouseLeftButtonDoubleClick(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_RBUTTONDOWN:
            return m->Actual::mouseRightButtonDown(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_RBUTTONUP:
            return m->Actual::mouseRightButtonUp(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_RBUTTONDBLCLK:
            return m->Actual::mouseRightDoubleClick(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_MOUSEWHEEL:
            return m->Actual::mouseWheel(
                       GET_WHEEL_DELTA_WPARAM(wParam),
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};

        case WM_DISPLAYCHANGE:
            return m->Actual::displayChange(
                       static_cast<uint32_t>(wParam),
                       Dimension{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}),
                   LRESULT{};

        case WM_COMMAND: return handleCommand(actual, msg);
        case WM_APPCOMMAND:
            return m->Actual::appCommand(
                GET_APPCOMMAND_LPARAM(lParam),
                {GET_DEVICE_LPARAM(lParam), GET_KEYSTATE_LPARAM(lParam)});
        case WM_SYSCOMMAND:
            return m->Actual::sysCommand(
                wParam & 0xFFF0, Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        case WM_TIMER:
            return m->Actual::timer(window, wParam, reinterpret_cast<TIMERPROC>(lParam)), LRESULT{};

        case WM_DPICHANGED:
            return m->Actual::dpiChanged(Rect::fromRECT(*reinterpret_cast<LPRECT>(lParam))),
                   LRESULT{};

        case WM_DEVICECHANGE: return m->Actual::deviceChange(wParam, lParam);

        case WM_POWERBROADCAST: return handlePowerBroadcast(actual, msg);

        default:
            if (message >= WM_USER) return m->Actual::userMessage(msg);
        }
        return {}; // unhandled
    }
    static auto handleCommand(void *actual, const WindowMessage &msg) -> OptLRESULT {
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (HIWORD(wParam)) {
        case 0: return m->Actual::menuCommand(LOWORD(wParam));
        case 1: return m->Actual::acceleratorCommand(LOWORD(wParam));
        default: return m->Actual::controlCommand({HIWORD(wParam), LOWORD(wParam), lParam});
        }
    }
    static auto handlePowerBroadcast(void *actual, const WindowMessage &msg) -> OptLRESULT {
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (wParam) {
        case PBT_APMPOWERSTATUSCHANGE: return m->Actual::powerStatusChange(), LRESULT{};
        case PBT_APMRESUMEAUTOMATIC: return m->Actual::powerResumeAutomatic(), LRESULT{};
        case PBT_APMRESUMESUSPEND: return m->Actual::powerResumeSuspend(), LRESULT{};
        case PBT_APMSUSPEND: return m->Actual::powerSuspend(), LRESULT{};
        case PBT_POWERSETTINGCHANGE:
            return m->Actual::powerSettingChange(
                       reinterpret_cast<POWERBROADCAST_SETTING *>(lParam)),
                   LRESULT{};
        }
        return {};
    }
};

} // namespace win32
