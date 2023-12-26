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
struct WindowPosition {
    HWND hwnd;
    HWND insertAfter;
    Point topLeft;
    Dimension dimension;
    UINT flags;

    constexpr bool hasZOrder() const { return 0 == (flags & SWP_NOZORDER); }
    constexpr bool hasTopLeft() const { return 0 == (flags & SWP_NOREPOSITION); }
    constexpr bool hasDimension() const { return 0 == (flags & SWP_NOSIZE); }

    constexpr static auto fromWINDOWPOS(WINDOWPOS const &winPos) -> WindowPosition {
        return {
            .hwnd = winPos.hwnd,
            .insertAfter = winPos.hwndInsertAfter,
            .topLeft = Point{winPos.x, winPos.y},
            .dimension = Dimension{winPos.cx, winPos.cy},
            .flags = winPos.flags,
        };
    }
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
struct WindowMessageHandler {
#pragma warning(push)
#pragma warning(disable : 4100) // unereference parameter - its here for documentation only!
    virtual void create(CREATESTRUCT *) {}
    virtual void destroy() {}
    virtual void size(const Dimension &, uint32_t) {}
    virtual void move(const Point &topLeft) {}
    virtual bool position(const WindowPosition &) {
        return false; // note: move & size is not called if true is returned!
    }
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
    virtual void displayChange(uint32_t bitsPerPixel, Dimension) {}

    // Input handling
    virtual bool inputKeyDown(uint32_t keyCode) { return false; }
    virtual bool inputKeyUp(uint32_t keyCode) { return false; }
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
    virtual bool mouseRightButtonUp(const Point &mousePosition, DWORD keyState) { return false; }
    virtual void mouseRightDoubleClick(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonDown(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonUp(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseMiddleButtonDoubleClick(const Point &mousePosition, DWORD keyState) {}
    virtual void mouseWheel(int wheelDelta, const Point &mousePosition, DWORD keyState) {}
    virtual void mouseExtraButtonDown() {}
    virtual void mouseExtraButtonUp() {}
    virtual void mouseExtraButtonDoubleClick() {}
    virtual void contextMenu(const Point &position) {}
    // …

    virtual void dpiChanged(const Rect &) {}

    virtual auto deviceChange(WPARAM event, LPARAM pointer) -> LRESULT { return {}; }

    virtual auto sysCommand(uint16_t command, const Point &mousePosition) -> OptLRESULT { return {}; }
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
        static constexpr auto fromBool = [](bool b) -> OptLRESULT { return b ? OptLRESULT{LRESULT{}} : OptLRESULT{}; };
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (message) {
        case WM_CREATE: return m->create(std::bit_cast<CREATESTRUCT *>(lParam)), LRESULT{};
        case WM_DESTROY: return m->destroy(), LRESULT{};
        case WM_SIZE:
            return m->size(Dimension{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, static_cast<uint32_t>(wParam)),
                   LRESULT{};
        case WM_MOVE: return m->move(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}), LRESULT{};
        case WM_WINDOWPOSCHANGED:
            return fromBool(m->position(WindowPosition::fromWINDOWPOS(*std::bit_cast<WINDOWPOS *>(lParam))));
        case WM_SETFOCUS: return m->setFocus(), LRESULT{};
        case WM_KILLFOCUS: return m->killFocus(), LRESULT{};

        case WM_PAINT: return fromBool(m->paint());
        case WM_CLOSE: return m->close(), LRESULT{};

        case WM_KEYDOWN: return fromBool(m->inputKeyDown(static_cast<uint32_t>(wParam)));
        case WM_KEYUP: return fromBool(m->inputKeyUp(static_cast<uint32_t>(wParam)));

        case WM_MOUSEMOVE:
            return m->mouseMove(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONDOWN:
            return m->mouseLeftButtonDown(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONUP:
            return m->mouseLeftButtonUp(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_LBUTTONDBLCLK:
            return m->mouseLeftButtonDoubleClick(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_RBUTTONDOWN:
            return m->mouseRightButtonDown(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_RBUTTONUP:
            return fromBool(
                m->mouseRightButtonUp(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)));
        case WM_RBUTTONDBLCLK:
            return m->mouseRightDoubleClick(
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_MOUSEWHEEL:
            return m->mouseWheel(
                       GET_WHEEL_DELTA_WPARAM(wParam),
                       Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                       GET_KEYSTATE_WPARAM(wParam)),
                   LRESULT{};
        case WM_CONTEXTMENU: return m->contextMenu(Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}), LRESULT{};

        case WM_DISPLAYCHANGE:
            return m->displayChange(
                       static_cast<uint32_t>(wParam), Dimension{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}),
                   LRESULT{};

        case WM_COMMAND: return handleCommand(actual, msg);
        case WM_APPCOMMAND:
            return m->appCommand(
                GET_APPCOMMAND_LPARAM(lParam), {GET_DEVICE_LPARAM(lParam), GET_KEYSTATE_LPARAM(lParam)});
        case WM_SYSCOMMAND: return m->sysCommand(wParam & 0xFFF0, Point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});

        case WM_TIMER: return m->timer(window, wParam, reinterpret_cast<TIMERPROC>(lParam)), LRESULT{};

        case WM_DPICHANGED: return m->dpiChanged(Rect::fromRECT(*reinterpret_cast<LPRECT>(lParam))), LRESULT{};

        case WM_DEVICECHANGE: return m->deviceChange(wParam, lParam);

        case WM_POWERBROADCAST: return handlePowerBroadcast(actual, msg);

        case WM_NCHITTEST: {
            auto result = ::DefWindowProc(window, message, wParam, lParam);
            return result;
        }

        default:
            if (message >= WM_USER) return m->Actual::userMessage(msg);
        }
        return {}; // unhandled
    }
    static auto handleCommand(void *actual, const WindowMessage &msg) -> OptLRESULT {
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (HIWORD(wParam)) {
        case 0: return m->menuCommand(LOWORD(wParam));
        case 1: return m->acceleratorCommand(LOWORD(wParam));
        default: return m->controlCommand({HIWORD(wParam), LOWORD(wParam), lParam});
        }
    }
    static auto handlePowerBroadcast(void *actual, const WindowMessage &msg) -> OptLRESULT {
        auto m = reinterpret_cast<Actual *>(actual);
        auto &[window, message, wParam, lParam] = msg;
        switch (wParam) {
        case PBT_APMPOWERSTATUSCHANGE: return m->powerStatusChange(), LRESULT{};
        case PBT_APMRESUMEAUTOMATIC: return m->powerResumeAutomatic(), LRESULT{};
        case PBT_APMRESUMESUSPEND: return m->powerResumeSuspend(), LRESULT{};
        case PBT_APMSUSPEND: return m->powerSuspend(), LRESULT{};
        case PBT_POWERSETTINGCHANGE:
            return m->powerSettingChange(reinterpret_cast<POWERBROADCAST_SETTING *>(lParam)), LRESULT{};
        }
        return {};
    }
};

} // namespace win32
