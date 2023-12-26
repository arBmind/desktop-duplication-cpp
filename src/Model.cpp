#include "Model.h"

#include <algorithm>

namespace deskdup {

auto State::computeIndexForMonitorHandle(HMONITOR monitor) -> MonitorIndex {
    auto it = std::find_if(monitors.cbegin(), monitors.cend(), [=](Monitor const &m) { return m.handle == monitor; });
    if (it == monitors.cend()) return 0;
    return static_cast<MonitorIndex>(it - monitors.cbegin());
}

auto PresentMirrorLens::captureAreaRect(State const &m) -> Rect {
    auto dim = m.config.outputDimension;
    auto zoom = m.config.outputZoom;
    auto offset = m.config.captureOffset;
    auto topLeft = m.captureMonitorRect.topLeft;
    return Rect{
        Point{
            static_cast<int>(topLeft.x - offset.x - 0.1),
            static_cast<int>(topLeft.y - offset.y - 0.1),
        },
        Dimension{
            static_cast<int>(1.5 + dim.width / zoom),
            static_cast<int>(1.5 + dim.height / zoom),
        },
    };
}

auto CaptureAreaLens::captureOffset(const State &m) -> Vec2f {
    auto topLeft = m.captureMonitorRect.topLeft;
    return Vec2f{
        topLeft.x - static_cast<float>(m.config.outputTopLeft.x),
        topLeft.y - static_cast<float>(m.config.outputTopLeft.y),
    };
}

} // namespace deskdup
