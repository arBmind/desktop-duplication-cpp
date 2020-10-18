#pragma once
#include "stable.h"

struct FrameContext {
    POINT offset; // offset from desktop to target coordinates
    DXGI_OUTPUT_DESC output_desc; // description of the frame
};
