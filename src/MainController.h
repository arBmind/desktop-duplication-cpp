#pragma once
#include "Model.h"

#include "win32/Geometry.h"

namespace deskdup {

using win32::Dimension;
using win32::Point;
using win32::Rect;

struct MainController {
    MainController() = default;
    MainController(MainController const &) = delete;
    MainController &operator=(MainController const &) = delete;
    virtual ~MainController() = default;

    virtual auto state() const -> State const & = 0;
    auto config() const -> Config const { return state().config; }
    auto operatonModeLens() const -> OperationModeLens const { return OperationModeLens{state()}; }

    virtual void quit() = 0;
    virtual void changeOperationMode(OperationMode) = 0;
    virtual void updateSystemStatus(SystemStatus) = 0;
    virtual void captureScreen(int) = 0;
    virtual void updateScreenRect(Rect) = 0;
    virtual void resizeOutputWindow(Dimension, bool isMaximized) = 0;
    virtual void moveOutputWindowTo(Point) = 0;
    virtual void moveCaptureOffsetByScreen(int dx, int dy) = 0;
    virtual void resetOutputZoom() = 0;
    virtual void zoomOutputBy(float) = 0;
    virtual void zoomOutputAt(Point, float) = 0;
    virtual void toggleVisualizeCaptureArea() = 0;
    virtual void changeDuplicationStatus(DuplicationStatus status) = 0;
    virtual void toggleOutputMaximize() = 0;
    virtual void refreshMonitors() = 0;

    void togglePause() {
        using enum DuplicationStatus;
        changeDuplicationStatus(state().duplicationStatus != Pause ? Pause : Live);
    }
};

} // namespace deskdup
