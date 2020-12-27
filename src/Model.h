#pragma once
#include "win32/Geometry.h"

namespace deskdup {

using win32::Dimension;
using win32::Point;
using win32::Rect;
using win32::Vec2f;

enum class SystemStatus {
    Neutral,
    Green,
    Yellow,
    Red,
};
enum class CaptureStatus {
    Disabled,
    Error,
    Enabling,
    Enabled,
};
enum class DuplicationStatus {
    Live,
    Paused,
    Blacked,
};

struct CaptureModel {
    int monitor() const { return m_monitor; }
    auto virtualRect() const -> Rect { return m_virtualRect; }
    auto status() const -> CaptureStatus { return m_status; }

    void setVirtualRect(const Rect &rect) { m_virtualRect = rect; }
    void setStatus(CaptureStatus status) { m_status = status; }

private:
    int m_monitor{};
    Rect m_virtualRect{};
    CaptureStatus m_status{};
};

struct DuplicationModel {
    auto outputRect() const -> const Rect & { return m_outputRect; }
    auto status() const -> DuplicationStatus { return m_status; }
    bool isFreezed() const { return m_status != DuplicationStatus::Live; }
    bool isMaximized() const { return m_isMaximized; }
    auto zoom() const -> float { return m_zoom; }
    auto captureOffset() const -> const Vec2f & { return m_captureOffset; }

    void setOutputRect(const Rect &rect) { m_outputRect = rect; }
    void setOutputTopLeft(const Point &topLeft) { m_outputRect.topLeft = topLeft; }
    void setOutputDimension(const Dimension &dimension) { m_outputRect.dimension = dimension; }

    void setStatus(DuplicationStatus status) { m_status = status; }
    void toggleFreezed() {
        if (m_status == DuplicationStatus::Paused) {
            m_status = DuplicationStatus::Live;
        }
        else if (m_status == DuplicationStatus::Live) {
            m_status = DuplicationStatus::Paused;
        }
    }

    void toggleMaximized() { m_isMaximized ^= true; }
    void setIsMaximized(bool value) { m_isMaximized = value; }

    void resetZoom() { m_zoom = 1; }
    void zoomBy(float delta) { m_zoom += delta; }
    void zoomAtScreen(const Point &point, float delta) {
        auto mx = point.x - outputRect().left();
        auto my = point.y - outputRect().top();
        auto offsetPoint = Vec2f{
            static_cast<float>(mx / m_zoom),
            static_cast<float>(my / m_zoom),
        };
        m_zoom += delta;
        m_captureOffset.x -= offsetPoint.x - static_cast<float>(mx / m_zoom);
        m_captureOffset.y -= offsetPoint.y - static_cast<float>(my / m_zoom);
    }

    void setCaptureOffset(const Vec2f &vec2f) { m_captureOffset = vec2f; }
    void moveCaptureOffsetByScreen(int dx, int dy) {
        m_captureOffset.x += static_cast<float>(dx) / m_zoom;
        m_captureOffset.y += static_cast<float>(dy) / m_zoom;
    }

private:
    Rect m_outputRect{};
    DuplicationStatus m_status{};
    bool m_isMaximized{};
    float m_zoom{1};
    Vec2f m_captureOffset{};
};

struct VisibleAreaModel {
    bool isShown() const { return m_shown; }
    auto rect() const -> const Rect & { return m_rect; }

    void toggleShown() { m_shown ^= true; }
    void setRect(const Rect &rect) { m_rect = rect; }

private:
    Rect m_rect{{0, 0}, {100, 100}};
    bool m_shown{true};
};

struct Model {
    constexpr Model() = default;

    auto status() const -> SystemStatus { return m_status; }

    auto capture() -> CaptureModel & { return m_capture; }
    auto duplication() -> DuplicationModel & { return m_duplication; }
    auto visibleArea() -> VisibleAreaModel & { return m_visibleArea; }

    auto updateStatus() {
        m_status = [this] {
            auto fromDuplicatonStatus = [this] {
                switch (duplication().status()) {
                case DuplicationStatus::Live: return SystemStatus::Green;
                case DuplicationStatus::Blacked:
                case DuplicationStatus::Paused: return SystemStatus::Yellow;
                }
                return SystemStatus::Red; // unknown
            };
            switch (capture().status()) {
            case CaptureStatus::Disabled: return SystemStatus::Neutral;
            case CaptureStatus::Error: return SystemStatus::Red;
            case CaptureStatus::Enabling: return SystemStatus::Neutral;
            case CaptureStatus::Enabled: return fromDuplicatonStatus();
            }
            return SystemStatus::Red; // unknown
        }();
    }

    void updateVisibleAreaRect() {
        auto outputRect = duplication().outputRect();
        auto zoom = duplication().zoom();
        auto offset = duplication().captureOffset();
        auto topLeft = capture().virtualRect().topLeft;
        auto visibleRect = Rect{
            Point{
                static_cast<int>(topLeft.x - offset.x - 0.1),
                static_cast<int>(topLeft.y - offset.y - 0.1),
            },
            Dimension{
                static_cast<int>(1.5 + outputRect.width() / zoom),
                static_cast<int>(1.5 + outputRect.height() / zoom),
            },
        };
        visibleArea().setRect(visibleRect);
    }

private:
    SystemStatus m_status{};
    CaptureModel m_capture{};
    DuplicationModel m_duplication{};
    VisibleAreaModel m_visibleArea{};
};

} // namespace deskdup
