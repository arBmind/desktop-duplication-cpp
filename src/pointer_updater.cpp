#include "pointer_updater.h"

void pointer_updater::update(const captured_update & data, const frame_context & context) {
	const auto &info = data.info;
	if (info.LastMouseUpdateTime.QuadPart == 0) return;
	if (pointer_desktop_m == context.output_desc.DesktopCoordinates
		|| (info.PointerPosition.Visible
			&& (!pointer_m.visible || info.LastMouseUpdateTime.QuadPart > pointer_m.position_timestamp))) {
		pointer_desktop_m = context.output_desc.DesktopCoordinates;
		pointer_m.position_timestamp = info.LastMouseUpdateTime.QuadPart;
		pointer_m.visible = info.PointerPosition.Visible;
		pointer_m.position.x = info.PointerPosition.Position.x + context.output_desc.DesktopCoordinates.left - context.offset.x;
		pointer_m.position.y = info.PointerPosition.Position.y + context.output_desc.DesktopCoordinates.top - context.offset.y;
	}
	if (!data.pointer_data.empty()) {
		pointer_m.shape_timestamp = info.LastMouseUpdateTime.QuadPart;
		pointer_m.shape_data = data.pointer_data;
		pointer_m.shape_info = data.pointer_info;
	}
}
