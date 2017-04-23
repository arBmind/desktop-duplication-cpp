#include "pointer_updater.h"

#include "captured_update.h"
#include "frame_context.h"

#include <utility>
#include <algorithm>

void pointer_updater::update(pointer_update& update, const frame_context& context) {
	if (update.update_time == 0) return;
	if (pointer_desktop_m == context.output_desc.DesktopCoordinates
		|| (update.position.Visible
			&& (!pointer_m.visible || update.update_time > pointer_m.position_timestamp))) {
		pointer_desktop_m = context.output_desc.DesktopCoordinates;
		pointer_m.position_timestamp = update.update_time;
		pointer_m.visible = update.position.Visible;
		pointer_m.position.x = update.position.Position.x + context.output_desc.DesktopCoordinates.left - context.offset.x;
		pointer_m.position.y = update.position.Position.y + context.output_desc.DesktopCoordinates.top - context.offset.y;
	}
	if (!update.shape_buffer.empty()) {
		pointer_m.shape_timestamp = update.update_time;
		pointer_m.shape_data = std::move(update.shape_buffer);
		pointer_m.shape_info = update.shape_info;
	}
}
