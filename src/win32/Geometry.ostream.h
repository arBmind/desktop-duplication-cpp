#pragma once
#include "Geometry.h"

#include <ostream>

namespace win32 {

auto operator<<(std::ostream &out, Point p) -> std::ostream & { return out << "[" << p.x << ", " << p.y << "]"; }

auto operator<<(std::ostream &out, Dimension d) -> std::ostream & {
    return out << "[" << d.width << ", " << d.height << "]";
}

auto operator<<(std::ostream &out, Rect r) -> std::ostream & {
    return out << "Rect{topLeft: " << r.topLeft << ", dimension: " << r.dimension << "}";
}

} // namespace win32
