#pragma once
#include <Windows.h>

namespace win32 {

/// win32 POINT replacement
struct Point {
    int x{};
    int y{};

    constexpr bool operator==(const Point &) const = default;

    /// construct Point from win32 POINT
    static constexpr auto fromPOINT(const POINT &point) -> Point { return {point.x, point.y}; }

    /// create a win32 POINT from this
    constexpr auto toPOINT() const -> POINT { return {x, y}; }
};

/// 2D vector of floating point coordinates
/// note: used for DirectX
struct Vec2f {
    float x{};
    float y{};
};

/// win32 SIZE replacement
struct Dimension {
    int width{};
    int height{};

    constexpr bool operator==(const Dimension &) const = default;

    static constexpr auto fromSIZE(const SIZE &size) -> Dimension { return {size.cx, size.cy}; }

    constexpr auto toSIZE() const -> SIZE { return {width, height}; }
};

/// win32 RECT replacement
/// note:
/// * stores topLeft & dimensions instead of topLeft and bottomRight
///   (this avoids possible confusion of the two points)
struct Rect {
    Point topLeft{};
    Dimension dimension{};

    constexpr bool operator==(const Rect &) const = default;

    /// construct from win32 RECT
    static constexpr auto fromRECT(const RECT &rect) -> Rect {
        return {
            Point{rect.left, rect.top},
            Dimension{rect.right - rect.left, rect.bottom - rect.top},
        };
    }
    /// construct from two win32 POINTs
    static constexpr auto fromPOINTS(const POINT &min, const POINT &max) -> Rect {
        return {
            Point{min.x, min.y},
            Dimension{max.x - min.x, max.y - min.y},
        };
    }

    constexpr auto contains(Point p) const -> bool {
        return left() <= p.x && right() > p.x && top() <= p.y && bottom() > p.y;
    }

    // convinience accessors
    constexpr auto left() const -> int { return topLeft.x; }
    constexpr auto top() const -> int { return topLeft.y; }
    constexpr auto width() const -> int { return dimension.width; }
    constexpr auto height() const -> int { return dimension.height; }

    constexpr auto bottom() const -> int { return top() + height(); }
    constexpr auto right() const -> int { return left() + width(); }

    /// accessor for the bottomRight point
    constexpr auto bottomRight() const -> Point { return {right(), bottom()}; }

    /// create win32 RECT from this
    constexpr auto toRECT() const -> RECT { return {left(), top(), right(), bottom()}; }
};

} // namespace win32
