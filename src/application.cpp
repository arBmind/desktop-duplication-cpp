#include "application.h"

#include "capture_thread.h"
#include "captured_update.h"
#include "renderer.h"
#include "frame_updater.h"
#include "pointer_updater.h"
#include "window_renderer.h"

#include "meta/tuple.h"
#include "meta/scope_guard.h"

#include <windowsx.h>

namespace {
constexpr const auto WINDOM_CLASS_NAME = L"desdup";

bool IsWindowMaximized(HWND windowHandle) {
  WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
  auto success = GetWindowPlacement(windowHandle, &placement);
  return success && placement.showCmd == SW_MAXIMIZE;
}

void ShowWindowBorder(HWND windowHandle, bool shown) {
  auto style = GetWindowLong(windowHandle, GWL_STYLE);
  const auto flags = WS_BORDER | WS_SIZEBOX | WS_DLGFRAME;
  if (shown) {
    style |= flags;
  }
  else {
    style &= ~flags;
  }

  SetWindowLong(windowHandle, GWL_STYLE, style);
}
uint32_t GetModifierKeys() {
  auto output = 0u;
  if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) output |= MK_SHIFT;
  if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) output |= MK_CONTROL;
  if ((GetKeyState(VK_MENU) & 0x8000) != 0) output |= MK_ALT;
  return output;
}
}

struct internal : capture_thread::callbacks {
  using config = application::config;

  internal(config&& cfg) : config_m(std::move(cfg)) {}

  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    static internal* self = nullptr;
    static auto armed = false;
    static auto dragging = false;
    static LPARAM lastpos;
    switch (message) {
    case WM_CREATE:
      if (nullptr == self) {
        auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<internal*>(create_struct->lpCreateParams);
      }
      break;
      //case WM_DESTROY: //???
    case WM_CLOSE:
      if (self->windowHandle_m == window) {
        PostQuitMessage(0);
      }
      break;

    case WM_SIZE:
      if (self->windowHandle_m == window) {
        self->handleSizeChanged(SIZE{ LOWORD(lParam), HIWORD(lParam) }, (uint32_t)wParam);
      }
      break;

    case WM_LBUTTONDBLCLK:
      self->toggleMaximized();
      break;

      // Zoom
    case WM_MOUSEWHEEL:
      if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL)) {
        auto wheel_delta = float(GET_WHEEL_DELTA_WPARAM(wParam))/(30*WHEEL_DELTA);
        if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)) {
          wheel_delta /= 10.f;
        }
        self->changeZoom(wheel_delta);
      }
      break;

      // Offset Dragging
    case WM_LBUTTONDOWN:
      if (GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
        dragging = true;
        lastpos = lParam;
      }
      break;
    case WM_LBUTTONUP:
      dragging = false;
      break;
    case WM_MOUSEMOVE:
      if (dragging && GET_KEYSTATE_WPARAM(wParam) == (MK_LBUTTON | MK_SHIFT)) {
        auto delta = POINT{
            GET_X_LPARAM(lParam) - GET_X_LPARAM(lastpos),
            GET_Y_LPARAM(lParam) - GET_Y_LPARAM(lastpos) };
        self->moveTexture(delta);
        lastpos = lParam;
      }
      break;

      // Fit Other Window
    case WM_RBUTTONDBLCLK:
      armed ^= true;
      break;
    case WM_KILLFOCUS:
      if (armed) {
        armed = false;
        auto other_window = GetForegroundWindow();
        self->fitWindow(other_window);
      }
      break;

      // Keyboard
    case WM_KEYDOWN: {
      auto mk = GetModifierKeys();
      switch (wParam) {
      case VK_UP:
        if (mk == MK_SHIFT) self->moveTextureTo(0, -1);
        break;
      case VK_DOWN:
        if (mk == MK_SHIFT) self->moveTextureTo(0, +1);
        break;
      case VK_LEFT:
        if (mk == MK_SHIFT) self->moveTextureTo(-1, 0);
        break;
      case VK_RIGHT:
        if (mk == MK_SHIFT) self->moveTextureTo(+1, 0);
        break;
      default: {
        auto ch = MapVirtualKey(uint32_t(wParam), MAPVK_VK_TO_CHAR);
        switch (ch) {
        case '0':
          if (mk == MK_CONTROL) self->setZoom(1.f);
          else if (mk & MK_CONTROL) self->setZoom(1.f);
          break;
        case '+':
          if (mk == MK_CONTROL) self->changeZoom(0.01f);
          else if (mk & MK_CONTROL) self->changeZoom(0.001f);
          break;
        case '-':
          if (mk == MK_CONTROL) self->changeZoom(-0.01f);
          else if (mk & MK_CONTROL) self->changeZoom(-0.001f);
          break;
        } // switch
      }
      } // switch
      break;
    }
      // DPI Aware
    case WM_DPICHANGED: {
      auto* new_rect = (LPRECT)lParam;
      auto new_size = rectSize(*new_rect);
      MoveWindow(window,
                 new_rect->left, new_rect->top,
                 new_size.cx, new_size.cy,
                 true);
      break;
    }
      // Visible Area
    case WM_RBUTTONUP:
      if (0 != (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT)) {
        self->toggleVisibleArea();
      }
      break;
    case WM_PAINT:
      if (window == self->visibleAreaWindow_m) {
        self->paintVisibleArea();
        break;
      }
      // fall through
    default:
      return DefWindowProc(window, message, wParam, lParam);
    }
    return 0;
  }

  constexpr static auto colorKey = RGB(0xFF, 0x20, 0xFF);
  void toggleVisibleArea() {
    if (nullptr == visibleAreaWindow_m) {
      {
        auto exStyle = WS_EX_LAYERED | WS_EX_TOPMOST;
        auto style = WS_POPUP;
        auto hasMenu = false;

        auto rect = visibleAreaRect();
        AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
        auto w = 4 + rect.right - rect.left,
            h = 4 + rect.bottom - rect.top,
            x = rect.left - 2,
            y = rect.top - 2;

        auto windowName = nullptr;
        auto wndParent = windowHandle_m;
        auto menu = nullptr;
        auto instance = GetModuleHandle(nullptr);
        auto param = nullptr;
        visibleAreaWindow_m = CreateWindowEx(exStyle, WINDOM_CLASS_NAME, windowName, style,
                                             x, y, w, h,
                                             wndParent, menu, instance, param);
      }
      {
        auto alpha = 0u;
        auto flags = LWA_COLORKEY;
        SetLayeredWindowAttributes(visibleAreaWindow_m, colorKey, alpha, flags);
      }
      ShowWindow(visibleAreaWindow_m, SW_SHOW);
      UpdateWindow(visibleAreaWindow_m);
    }
    else {
      DestroyWindow(visibleAreaWindow_m);
      visibleAreaWindow_m = nullptr;
    }
  }

  void updateVisibleArea() {
    if (nullptr == visibleAreaWindow_m) return;

    auto rect = visibleAreaRect();
    auto size = rectSize(rect);
    auto repaint = true;
    MoveWindow(visibleAreaWindow_m, rect.left - 2, rect.top - 2, size.cx + 4, size.cy + 4, repaint);
  }

  void paintVisibleArea() {
    RECT window_rect;
    GetClientRect(visibleAreaWindow_m, &window_rect);
    auto window_size = rectSize(window_rect);

    PAINTSTRUCT p;
    auto dc = BeginPaint(visibleAreaWindow_m, &p);

    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    SelectObject(dc, GetStockObject(DC_PEN));

    SetDCPenColor(dc, RGB(255, 80, 40));
    Rectangle(dc, 0, 0, window_size.cx, window_size.cy);

    SelectObject(dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(dc, colorKey);
    SetDCPenColor(dc, RGB(255, 255, 255));
    Rectangle(dc, 1, 1, window_size.cx - 1, window_size.cy - 1);

    EndPaint(visibleAreaWindow_m, &p);
  }

  void handleSizeChanged(const SIZE& size, const uint32_t flags) {
    if (!duplicationStarted_m) return;
    setMaximized((flags & SIZE_MAXIMIZED) != 0);
    RECT window_rect;
    if (maximized_m) GetWindowRect(windowHandle_m, &window_rect);
    else GetClientRect(windowHandle_m, &window_rect);
    auto window_size = rectSize(window_rect);
    auto changed = windowRenderer_m.resize(window_size);
    if (changed) {
      doRender_m = true;
      updateVisibleArea();
    }
  }

  bool maximized_m = false;
  void setMaximized(bool maximized) {
    if (maximized == maximized_m) return;
    maximized_m = maximized;
    bool success = true;
    if (maximized_m) {
      success = PowerSetRequest(powerHandle_m, PowerRequestDisplayRequired)
        && PowerSetRequest(powerHandle_m, PowerRequestSystemRequired);
    }
    else {
      success = PowerClearRequest(powerHandle_m, PowerRequestDisplayRequired)
        && PowerClearRequest(powerHandle_m, PowerRequestSystemRequired);
    }
    if (!success) throw Unexpected{ "Failed to cast power request" };
  }

  void changeZoom(float zoomDelta) {
    setZoom(windowRenderer_m.zoom() + zoomDelta);
    updateVisibleArea();
  }
  void setZoom(float zoom) {
    if (!duplicationStarted_m) return;
    windowRenderer_m.setZoom(std::max(zoom, 0.05f));
    updateVisibleArea();
    doRender_m = true;
  }

  void toggleMaximized() {
    ::ShowWindow(windowHandle_m, IsWindowMaximized(windowHandle_m) ? SW_SHOWNORMAL : SW_MAXIMIZE);
    updateVisibleArea();
  }

  void moveTexture(POINT delta) {
    windowRenderer_m.moveOffset(delta);
    updateVisibleArea();
    doRender_m = true;
  }
  void moveTextureTo(int x, int y) {
    windowRenderer_m.moveToBorder(x, y);
    updateVisibleArea();
    doRender_m = true;
  }

  RECT visibleAreaRect() {
    RECT window_rect;
    GetClientRect(windowHandle_m, &window_rect);
    auto window_size = rectSize(window_rect);
    auto zoom = windowRenderer_m.zoom();
    auto renderOffset = windowRenderer_m.offset();
    auto left = offset_m.x - renderOffset.x;
    auto top = offset_m.y - renderOffset.y;
    auto width = long(window_size.cx / zoom);
    auto height = long(window_size.cy / zoom);
    return RECT{ left, top, left + width, top + height };
  }

  void fitWindow(HWND otherWindow) {
    RECT other_rect;
    GetWindowRect(otherWindow, &other_rect);
    auto rect = visibleAreaRect();
    auto size = rectSize(rect);
    auto repaint = true;
    MoveWindow(otherWindow, rect.left, rect.top, size.cx, size.cy, repaint);
  }

  static void registerWindowClass(HINSTANCE instanceHandle) {
    auto cursor = LoadCursor(nullptr, IDC_CROSS);
    if (!cursor) throw Unexpected{ "LoadCursor failed" };
    LATER(DestroyCursor(cursor));

    auto icon = LoadIcon(instanceHandle, L"desk1");

    WNDCLASSEXW window_class;
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = windowProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instanceHandle;
    window_class.hIcon = icon;
    window_class.hCursor = cursor;
    window_class.hbrBackground = nullptr;
    window_class.lpszMenuName = nullptr;
    window_class.lpszClassName = WINDOM_CLASS_NAME;
    window_class.hIconSm = icon;

    auto success = RegisterClassExW(&window_class);
    if (!success) throw Unexpected{ "Window class registration failed" };
  }

  HWND createMainWindow(HINSTANCE instanceHandle, int showCommand) {
    auto rect = RECT{ 100, 100, 924, 678 };
    auto style = WS_OVERLAPPEDWINDOW;
    auto has_menu = false;
    AdjustWindowRect(&rect, style, has_menu);

    static const auto title = L"Duplicate Desktop Presenter (Double Click to toggle fullscreen)";
    auto size = rectSize(rect);
    auto parent_window = nullptr;
    auto menu = nullptr;
    auto custom_param = this;
    auto window_handle = CreateWindowW(WINDOM_CLASS_NAME, title,
                                       style,
                                       rect.left, rect.top,
                                       size.cx, size.cy,
                                       parent_window, menu, instanceHandle, custom_param);
    if (!window_handle) throw Unexpected{ "Window creation failed" };

    ShowWindow(window_handle, showCommand);
    UpdateWindow(window_handle);

    return window_handle;
  }

  static void CALLBACK setErrorAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr<internal*, std::exception_ptr>(parameter);
    std::get<internal*>(*args)->updateError(std::get<1>(*args));
  }
  static void CALLBACK setFrameAPC(ULONG_PTR parameter) {
    auto args = unique_tuple_ptr<internal*, captured_update, std::reference_wrapper<frame_context>, int>(parameter);
    std::get<internal*>(*args)->updateFrame(std::get<1>(*args), std::get<2>(*args), std::get<3>(*args));
  }
  static void CALLBACK retryDuplicationAPC(LPVOID parameter, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
    auto self = reinterpret_cast<internal*>(parameter);
    self->canStartDuplication_m = true;
  }
  static void CALLBACK awaitFrameAPC(LPVOID parameter, DWORD dwTimerLowValue, DWORD dwTimerHighValue) {
    auto self = reinterpret_cast<internal*>(parameter);
    self->waitFrame_m = false;
  }

  void initCaptureThreads() {
    for (auto display : config_m.displays) {
      capture_thread::init_args args;
      args.callbacks = this;
      args.display = display;
      args.thread_index = captureThreads_m.size();
      captureThreads_m.emplace_back(new capture_thread(std::move(args)));
    }
    threads_m.reserve(config_m.displays.size());
  }

  int run() {
    try {
      registerWindowClass(config_m.instanceHandle);
      threadHandle_m = GetCurrentThreadHandle();
      LATER(CloseHandle(threadHandle_m));
      powerHandle_m = createPowerRequest();
      LATER(setMaximized(false); CloseHandle(powerHandle_m));
      retryTimer_m = CreateWaitableTimer(nullptr, false, TEXT("RetryTimer"));
      LATER(CloseHandle(retryTimer_m));
      frameTimer_m = CreateWaitableTimer(nullptr, false, TEXT("FrameTimer"));
      LATER(CloseHandle(frameTimer_m));

      windowHandle_m = createMainWindow(config_m.instanceHandle, config_m.showCommand);
      initCaptureThreads();

      return mainLoop();
    }
    catch (const Unexpected& e) {
      OutputDebugStringA(e.text);
      OutputDebugStringA("\n");
      return -1;
    }
  }

  HANDLE createPowerRequest() {
    REASON_CONTEXT reason;
    reason.Version = POWER_REQUEST_CONTEXT_VERSION;
    reason.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    reason.Reason.SimpleReasonString = L"Desktop Duplication Tool";
    return PowerCreateRequest(&reason);
  }

  int mainLoop() {
    while (keepRunning_m) {
      try {
        if (!canStartDuplication_m) {
          sleep();
          continue;
        }
        startDuplication();
      }
      catch (Expected& e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        stopDuplication();
        awaitRetry();
        continue;
      }
      try {
        while (keepRunning_m && duplicationStarted_m && !hasError_m) {
          render();
          sleep();
        }
        rethrow();
      }
      catch (Expected& e) {
        OutputDebugStringA(e.text);
        OutputDebugStringA("\n");
        error_m = {};
        stopDuplication();
      }
    }
    stopDuplication();
    return returnValue_m;
  }

  void startDuplication() {
    if (duplicationStarted_m) return;
    duplicationStarted_m = true;
    try {
      auto device_value = renderer::createDevice();
      auto device = device_value.device;
      auto device_context = device_value.deviceContext;

      auto dimensions = renderer::getDimensionData(device, config_m.displays);

      target_m = renderer::createSharedTexture(device, rectSize(dimensions.rect));

      window_renderer::init_args window_args;
      window_args.windowHandle = windowHandle_m;
      window_args.texture = target_m;
      window_args.device = device;
      window_args.deviceContext = device_context;
      windowRenderer_m.init(std::move(window_args));

      offset_m = { dimensions.rect.left, dimensions.rect.top };
      auto handle = renderer::getSharedHandle(target_m);
      for (auto& capture : captureThreads_m) {
        startCaptureThread(*capture, offset_m, handle);
      }
    }
    catch (const renderer::error& e) {
      throw Expected{ e.message };
    }
  }

  void startCaptureThread(capture_thread& capture, const POINT& offset, HANDLE targetHandle) {
    auto device_value = renderer::createDevice();
    auto device = device_value.device;
    auto device_context = device_value.deviceContext;

    frame_updater::init_args updater_args;
    updater_args.device = device;
    updater_args.deviceContext = device_context;
    updater_args.targetHandle = targetHandle;
    frameUpdaters_m.emplace_back(std::move(updater_args));

    capture_thread::start_args thread_args;
    thread_args.device = device;
    thread_args.offset = offset;
    threads_m.emplace_back(capture.start(std::move(thread_args)));
  }

  void stopDuplication() {
    if (!duplicationStarted_m) return;
    try {
      for (auto& capture : captureThreads_m) {
        capture->stop();
      }
      for (auto& thread : threads_m) {
        thread.join();
      }
      threads_m.clear();
      target_m.Reset();
      frameUpdaters_m.clear();
      windowRenderer_m.reset();
    }
    catch (Expected& e) {
      OutputDebugStringA("stopDuplication Threw: ");
      OutputDebugStringA(e.text);
      OutputDebugStringA("\n");
    }
    duplicationStarted_m = false;
  }

  void render() {
    if (waitFrame_m || !doRender_m) return;
    doRender_m = false;
    ShowWindowBorder(windowHandle_m, !IsWindowMaximized(windowHandle_m));
    try {
      windowRenderer_m.render();
      windowRenderer_m.renderPointer(pointerUpdater_m.data());
      windowRenderer_m.swap();
      for (auto index : updatedThreads_m) captureThreads_m[index]->next();
      updatedThreads_m.clear();
      awaitNextFrame();
    }
    catch (const renderer::error& e) {
      throw Expected{ e.message };
    }
  }

  void awaitNextFrame() {
    if (!waitFrame_m) {
      waitFrame_m = true;
      LARGE_INTEGER queue_time;
      queue_time.QuadPart = -10 * 1000 * 10; // relative 10 ms
      auto noPeriod = 0;
      auto apcArgument = this;
      auto awakeSuspend = false;
      auto success = SetWaitableTimer(retryTimer_m, &queue_time, noPeriod, awaitFrameAPC, apcArgument, awakeSuspend);
      if (!success) throw Unexpected{ "failed to arm frame timer" };
    }
  }

  void awaitRetry() {
    if (canStartDuplication_m) {
      canStartDuplication_m = false;
      LARGE_INTEGER queue_time;
      queue_time.QuadPart = -250 * 1000 * 10; // relative 250 ms
      auto noPeriod = 0;
      auto apcArgument = this;
      auto awakeSuspend = false;
      auto success = SetWaitableTimer(frameTimer_m, &queue_time, noPeriod, retryDuplicationAPC, apcArgument, awakeSuspend);
      if (!success) throw Unexpected{ "failed to arm retry timer" };
    }
  }

  void sleep(int timeout = INFINITE) {
    auto wake_mask = QS_ALLINPUT;
    auto flags = MWMO_ALERTABLE | MWMO_INPUTAVAILABLE;
    auto awoken = MsgWaitForMultipleObjectsEx(
          (uint32_t)handles_m.size(), handles_m.data(),
          timeout, wake_mask, flags);
    if (WAIT_FAILED == awoken)
      throw Unexpected{ "Application Waiting failed" };
    if (WAIT_OBJECT_0 == awoken - handles_m.size())
      processMessages();
  }

  void processMessages() {
    while (true) {
      auto message = MSG{};
      auto success = PeekMessage(&message, nullptr, 0, 0, PM_REMOVE);
      if (!success) return;
      TranslateMessage(&message);
      DispatchMessage(&message);
      if (WM_QUIT == message.message) {
        keepRunning_m = false;
        returnValue_m = (int)message.wParam;
      }
    }
  }

  void rethrow() {
    if (!hasError_m) return;
    hasError_m = false;
    std::rethrow_exception(error_m);
  }

  void updateError(std::exception_ptr& exception) {
    error_m = exception;
    hasError_m = true;
  }
  void updateFrame(captured_update& update, const frame_context& context, int thread_index) {
    frameUpdaters_m[thread_index].update(update.frame, context);
    pointerUpdater_m.update(update.pointer, context);
    updatedThreads_m.push_back(thread_index);
    doRender_m = true;
  }

private:
  config config_m;
  HWND windowHandle_m;
  HANDLE powerHandle_m;
  HANDLE retryTimer_m;
  HANDLE frameTimer_m;
public:
  HANDLE threadHandle_m;
private:
  bool canStartDuplication_m = true;
  bool duplicationStarted_m = false;
  bool keepRunning_m = true;
  int returnValue_m = 0;
  bool hasError_m = false;
  std::exception_ptr error_m;
  bool waitFrame_m = false;
  bool doRender_m = false;

  POINT offset_m;
  window_renderer windowRenderer_m;
  ComPtr<ID3D11Texture2D> target_m;
  std::vector<frame_updater> frameUpdaters_m;
  pointer_updater pointerUpdater_m;

  using capture_thread_ptr = std::unique_ptr<capture_thread>;

  std::vector<HANDLE> handles_m;
  std::vector<std::thread> threads_m;
  std::vector<capture_thread_ptr> captureThreads_m;
  std::vector<int> updatedThreads_m;

  HWND visibleAreaWindow_m = nullptr;
};

struct application::internal: ::internal {
  using ::internal::internal;
};

application::application(config&& cfg)
  : ip(internal_ptr{ new internal(std::move(cfg)) }) {}

int application::run() {
  return ip->run();
}

void capture_thread::callbacks::setError(std::exception_ptr error) {
  auto self = reinterpret_cast<internal*>(this);
  auto parameter = make_tuple_ptr(self, error);
  auto success = QueueUserAPC(internal::setErrorAPC, self->threadHandle_m, (ULONG_PTR)parameter);
  if (!success) throw Unexpected{ "api::setError failed to queue APC" };
}

void capture_thread::callbacks::setFrame(captured_update&& frame, const frame_context& context, size_t thread_index) {
  auto self = reinterpret_cast<internal*>(this);
  auto parameter = make_tuple_ptr(self, std::move(frame), std::ref(context), thread_index);
  auto success = QueueUserAPC(internal::setFrameAPC, self->threadHandle_m, (ULONG_PTR)parameter);
  if (!success) throw Unexpected{ "api::setError failed to queue APC" };
}

void application::internal_deleter::operator()(internal *ptr) {
  delete ptr;
}
