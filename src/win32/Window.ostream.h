#pragma once
#include "Window.h"

#include "Geometry.ostream.h"

#include <ostream>

namespace win32 {

auto operator<<(std::ostream &out, const Window::ShowState &st) -> std::ostream & {
    switch (st) {
    case Window::ShowState::Normal: return out << "Normal";
    case Window::ShowState::Minimized: return out << "Minimized";
    case Window::ShowState::Maximized: return out << "Maximized";
    }
    return out << "[unknown]";
}

auto operator<<(std::ostream &out, const Window::Placement &p) -> std::ostream & {
    return out << "Placement{showState: " << p.showState << ","
               << " current: " << p.currentRect << ","
               << " normal: " << p.normalRect << "}";
}

} // namespace win32
