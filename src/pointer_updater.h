#pragma once
#include "stable.h"

#include "captured_update.h"
#include "frame_context.h"

#include <vector>

struct pointer_data {
	POINT position;
	bool visible = false;
	int64_t last_update = 0;
	std::vector<uint8_t> shape_data;
	DXGI_OUTDUPL_POINTER_SHAPE_INFO shape_info;
};

struct pointer_updater {

	void update(const captured_update &data, const frame_data& context);

	const pointer_data& data() const { return pointer_m; }

private:
	RECT pointer_desktop_m = { 0,0,0,0 };
	pointer_data pointer_m;
};