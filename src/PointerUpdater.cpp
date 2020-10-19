#include "PointerUpdater.h"

#include "CapturedUpdate.h"
#include "FrameContext.h"

#include <algorithm>
#include <utility>

void PointerUpdater::update(PointerUpdate &update, const FrameContext &context) {
    if (update.update_time == 0) return;
    if (m_pointer_desktop == context.output_desc.DesktopCoordinates //
        || (update.position.Visible &&
            (!m_pointer.visible || update.update_time > m_pointer.position_timestamp))) {
        m_pointer_desktop = context.output_desc.DesktopCoordinates;
        m_pointer.position_timestamp = update.update_time;
        m_pointer.visible = update.position.Visible;
        m_pointer.position.x = update.position.Position.x +
                               context.output_desc.DesktopCoordinates.left - context.offset.x;
        m_pointer.position.y = update.position.Position.y +
                               context.output_desc.DesktopCoordinates.top - context.offset.y;
    }
    if (!update.shape_buffer.empty()) {
        m_pointer.shape_timestamp = update.update_time;
        m_pointer.shape_data = std::move(update.shape_buffer);
        m_pointer.shape_info = update.shape_info;
    }
}
