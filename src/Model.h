#pragma once
#include "win32/DisplayMonitor.h"
#include "win32/Geometry.h"

#include <string>
#include <vector>

namespace deskdup {

using win32::Dimension;
using win32::MonitorInfo;
using win32::Point;
using win32::Rect;
using win32::Vec2f;

enum class SystemStatus {
    Neutral,
    Green,
    Yellow,
    Red,
};
enum class OperationMode {
    PresentMirror, ///< capture one area of screen and present the mirror somewhere else
    CaptureArea, ///< capture the same area and replay it for capture a window to pick it up
};

enum class CaptureVisualization {
    Border,
    // Stripes,
};
enum class DuplicationStatus {
    Live,
    Pause,
};
enum class PauseRendering {
    LastFrame,
    Black,
    White,
};
using MonitorIndex = int;

/// Data that should be stored from one session to the next
/// note: some sanitization is needed to avoid unusable state (like monitor does not exist)
struct Config final {
    OperationMode operationMode{};
    PauseRendering pauseRendering{};

    MonitorIndex captureMonitor{}; // 0 = main monitor
    Vec2f captureOffset{}; ///< relative to the capture monitor
    bool isCaptureAreaShown{};
    CaptureVisualization captureVisualization{};

    Point outputTopLeft{};
    Dimension outputDimension{};
    float outputZoom{1.0f};

    auto outputRect() const -> Rect { return Rect{outputTopLeft, outputDimension}; }
};

struct Monitor final {
    HMONITOR handle{};
    MonitorInfo info{};

    bool operator==(Monitor const &) const = default;
};
using Monitors = std::vector<Monitor>;

/// Data that is produced locally and is valid only at the current state
struct State final {
    Config config{};
    bool outputMaximized{};
    SystemStatus systemStatus{};
    DuplicationStatus duplicationStatus{};
    Monitors monitors{}; ///< all currently available monitors
    MonitorIndex outputMonitor{}; ///<
    Rect captureMonitorRect{}; ///<

    auto computeIndexForMonitorHandle(HMONITOR) -> MonitorIndex;
};

struct PresentMirrorLens final {
    static auto captureMonitor(State const &m) -> MonitorIndex { return m.config.captureMonitor; }
    static auto captureOffset(State const &m) -> Vec2f { return m.config.captureOffset; }
    static auto outputZoom(State const &m) -> float { return m.config.outputZoom; }

    // note: depends on captureMonitorRect, captureOffset, outputDimension, outputZoom
    static auto captureAreaRect(State const &m) -> Rect;
};

struct CaptureAreaLens final {
    static auto captureMonitor(State const &m) -> MonitorIndex { return m.outputMonitor; }
    static auto captureOffset(State const &m) -> Vec2f;
    static auto outputZoom(State const &) -> float { return 1.0f; }
    static auto captureAreaRect(State const &m) -> Rect { return m.config.outputRect(); }
};

/// Combination of the application data
/// Adds accessors that give different answers depending on the state of the program
struct OperationModeLens final {
    explicit OperationModeLens(State const &state)
        : m{state} {}

    auto state() const -> State const & { return m; }
    auto config() const -> Config const & { return m.config; }

    auto captureMonitor() const -> MonitorIndex {
        switch (m.config.operationMode) {
        case OperationMode::PresentMirror: return PresentMirrorLens::captureMonitor(m);
        case OperationMode::CaptureArea: return CaptureAreaLens::captureMonitor(m);
        }
        return {};
    }
    auto captureOffset() const -> Vec2f {
        switch (m.config.operationMode) {
        case OperationMode::PresentMirror: return PresentMirrorLens::captureOffset(m);
        case OperationMode::CaptureArea: return CaptureAreaLens::captureOffset(m);
        }
        return {};
    }
    auto outputZoom() const -> float {
        switch (m.config.operationMode) {
        case OperationMode::PresentMirror: return PresentMirrorLens::outputZoom(m);
        case OperationMode::CaptureArea: return CaptureAreaLens::outputZoom(m);
        }
        return {};
    }
    auto captureAreaRect() const -> Rect {
        switch (m.config.operationMode) {
        case OperationMode::PresentMirror: return PresentMirrorLens::captureAreaRect(m);
        case OperationMode::CaptureArea: return CaptureAreaLens::captureAreaRect(m);
        }
        return {};
    }

private:
    State const &m;
};

} // namespace deskdup
