#pragma once
#include "stable.h"

struct frame_context {
    POINT offset; // offset from desktop to target coordinates
    DXGI_OUTPUT_DESC output_desc; // description of the frame
};
