#pragma once
#include "Dpi.h"
#include "Geometry.h"
#include "WindowMessageHandler.h"

#include "meta/member_method.h"

#include <chrono>
#include <optional>
#include <string_view>

namespace win32 {

template<class T>
using Optional = std::optional<T>;

using Name = std::wstring;
using String = std::wstring;

struct Icon {
    HICON hIcon{};
};

struct Cursor {
    HCURSOR hCursor{};
};

struct Instance {
    HINSTANCE hInstance{};
};

struct Menu {
    HMENU hMenu{};
};

/// encapsulates normal and exteded window styles in one
struct WindowStyle {
    DWORD exStyle{};
    DWORD style{};

    static auto child() -> WindowStyle { return {0, WS_CHILD}; }
    static auto popup() -> WindowStyle { return {WS_EX_TOOLWINDOW, WS_POPUP}; }
    static auto overlappedWindow() -> WindowStyle { return {WS_EX_OVERLAPPEDWINDOW, WS_OVERLAPPEDWINDOW}; }

    auto topMost() const -> WindowStyle { return {exStyle | WS_EX_TOPMOST, style}; }
    auto transparent() const -> WindowStyle { return {exStyle | WS_EX_TRANSPARENT, style}; }
};

/// convinient wrapper to ease working with windows
struct Window {
    enum class ShowState { Normal, Minimized, Maximized };
    struct Placement {
        ShowState showState{};
        Rect currentRect{};
        Rect normalRect{};
    };
    /// Configuration used for construction of new windows
    /// use with WindowClass::createWindow
    struct Config {
        WindowStyle style = {};
        Name name = {};
        Rect rect = {};
        const Window *parent = {};
        Menu menu = {};
    };

    explicit Window() = default; ///< default constructs invalid window
    explicit Window(HWND hwnd)
        : m_windowHandle(hwnd) {}

    /// construct a Window wrapper for the current foreground window
    static auto fromForeground() -> Window { return Window{::GetForegroundWindow()}; }

    /// construct a Window wrapper for the desktop window
    static auto fromDesktop() -> Window { return Window{::GetDesktopWindow()}; }

    /// calls f with all windows
    // note: this is a template - therefore it is kept inline!
    template<class F>
    static void enumerate(F &&f) {
        struct Callback {
            static auto CALLBACK enumerateCallback(HWND hwnd, LPARAM lParam) -> BOOL {
                auto *fp = std::bit_cast<F *>(lParam);
                (*fp)(Window{hwnd});
                return true;
            }
        };
        ::EnumWindows(&Callback::enumerateCallback, std::bit_cast<LPARAM>(&f));
    }

    /// returns the internal handle
    /// use it to call non wrapped win32 APIs
    auto handle() const -> HWND { return m_windowHandle; }

    auto rect() const -> Rect; ///< returns outer window rect
    // note: requires extra dwn dll - enable if needed.
    // auto extendedFrameRect() const -> Rect; ///< returns border rect
    auto clientRect() const -> Rect;
    auto clientOffset() const -> Point;

    /// store current window placement
    auto placement() const -> Placement;
    auto isMaximized() const -> bool;
    auto isMinimized() const -> bool;

    auto style() const -> WindowStyle;
    auto isVisible() const -> bool; ///< true if Style has WS_VISIBLE (intended to draw)
    auto dpiAwareness() const -> DpiAwareness;
    auto dpi() const -> Dpi;

    /// return title or input text of window
    auto text() const -> String;

    void show(); ///< send show command to window (last state is kept)
    void hide(); ///< send hide command (will window will disapear)
    void toBottom();
    void toTop();

    void showNormal(); ///< send show normal (restores from maximized)
    void showMaximized(); ///< send show maximized command
    void showMinimized(); ///< send show minimized command

    void update(); ///< trigger WM_PAINT for window

    // note: some app windows may not follow the move/resize!
    void move(const Rect &); ///< move outer window rect
    void moveBorder(const Rect &); ///< move window visible border to rect
    void moveClient(const Rect &); ///< move window client rect
    void setPosition(const Point &); ///< move top left corner (keeping size)
    bool setPlacement(const Placement &); ///< restore a placement

    void styleNonLayered();
    void styleTransparent();
    void styleLayeredColorKey(uint32_t colorKey);
    void styleLayeredAlpha(uint8_t alpha);

private:
    friend struct WindowWithMessages;
    HWND m_windowHandle{};
};

/// Extended Wrapper for our own created windows
/// note:
/// * constructed by WindowClass::createWindow
/// * allows to set the message handler for this window
struct WindowWithMessages final : Window {
    WindowWithMessages(const WindowWithMessages &) = delete;
    WindowWithMessages &operator=(const WindowWithMessages &) = delete;
    WindowWithMessages(WindowWithMessages &&) = delete;
    WindowWithMessages &operator=(WindowWithMessages &&) = delete;
    ~WindowWithMessages() noexcept; ///< destroy the owned window

    /// defines the proper message handler
    template<class T>
    void setMessageHandler(WindowMessageHandler<T> *handler) {
        m_messageHandlerFunc = &handler->handleMessage;
        m_messageHandlerPtr = handler;
    }
    template<auto M, class T = MemberMethodClass<M>>
        requires(MemberMedthodSig<OptLRESULT(const WindowMessage &), decltype(M)>)
    void setCustomHandler(T *ptr) {
        m_messageHandlerFunc = [](void *ptr, const WindowMessage &msg) -> OptLRESULT {
            auto p = std::bit_cast<T *>(ptr);
            return (p->*M)(msg);
        };
        m_messageHandlerPtr = ptr;
    }
    /// restore the noop message handler
    void resetMessageHandler();

    auto handleMessage(const WindowMessage &msg) -> OptLRESULT;

private:
    using HandleFunc = auto(void *, const WindowMessage &) -> OptLRESULT;
    static auto noopMessageHandler(void *, const WindowMessage &) -> OptLRESULT;

    HandleFunc *m_messageHandlerFunc{&WindowWithMessages::noopMessageHandler};
    void *m_messageHandlerPtr{};

    friend struct WindowClass;
    WindowWithMessages(const Config &config, HINSTANCE instance, const Name &className);
};

/// Wrapper around our created WindowClass
struct WindowClass final {
    /// configuration to construct our windowClass
    /// note: avoids long argument lists
    struct Config {
        /// current application instance (required)
        /// note: you get it from WinMain or ::GetModuleHandle(nullptr)
        HINSTANCE instanceHandle = {};
        Icon icon = {};
        Icon smallIcon = {};
        Cursor cursor = {};
        Name className = {}; ///< class name used for WindowClass (required)
        Name menuName = {};

        bool isValid() const { return instanceHandle != nullptr && !className.empty(); }
    };
    WindowClass(Config);
    ~WindowClass() noexcept;

    /// instance handle used to create WindowClass
    /// note: use to call win32 APIs that need it
    auto instanceHandle() const { return m_instanceHandle; }

    /// construct a new window according to the provided config
    /// note: you should set a message handler and probably show the window
    auto createWindow(WindowWithMessages::Config) const -> WindowWithMessages;

    void recreateWindow(WindowWithMessages &wnd, WindowWithMessages::Config) const;

private:
    HINSTANCE m_instanceHandle{};
    Name m_className{};

    static auto registerWindowClass(const Config &config) -> ATOM;
    ATOM m_atom{};

    static auto CALLBACK staticWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT;
};

} // namespace win32
