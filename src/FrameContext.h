#pragma once
#include "win32/Geometry.h"

#include <dxgi1_3.h>

using win32::Point;

struct FrameContext {
    Point offset; // offset from desktop to target coordinates
    DXGI_OUTPUT_DESC output_desc; // description of the frame
};
