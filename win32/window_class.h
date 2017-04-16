#pragma once
#include "intern/flags.h"
#include <string>

namespace win32 {

struct window;
struct message;

struct window_class {
    using message_handler_func = intptr_t (*)(const window&, const message&);

    intern::WindowClassStyles styles;
    message_handler_func message_handler; // wndproc
    size_t class_extra_bytes;
    size_t window_extra_bytes;
//    instance_handle instance;
//    icon_handle icon;
//    cursor_handle cursor;
//    background_brush_handle background_brush;
//    std::wstring menu_name;
//    std::wstring class_name;
//    icon_handle small_icon;
};

} // namespace win32
